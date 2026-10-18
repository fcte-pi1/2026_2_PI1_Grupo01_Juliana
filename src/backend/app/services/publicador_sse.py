"""Fila de eventos SSE por cliente (BACK-07).

A ingestão roda no threadpool (a rota do POST /telemetria é síncrona) e os
clientes são consumidos no event loop. `asyncio.Queue` não é thread-safe, então
a publicação nunca toca na fila: ela agenda a entrega no loop que vai consumi-la,
com `call_soon_threadsafe`.
"""

import asyncio
import logging
import threading
from collections.abc import Sequence
from dataclasses import dataclass

logger = logging.getLogger("app.sse")

# cerca de 50 s de folga a 10 msg/s (RNF-B11) antes de dar o cliente por perdido
TAMANHO_FILA = 500

# sentinela que pede o fim do fluxo de um cliente
FIM = None


@dataclass(frozen=True, slots=True)
class EventoSSE:
    """Evento já serializado, pronto para virar frame. `id` só existe no passo."""

    tipo: str
    dados: str
    id: int | None = None
    # último evento da execução: depois dele não vem mais nada e o fluxo fecha
    encerra: bool = False


class Assinatura:
    """A fila de um cliente. Nasce na thread do event loop que vai consumi-la."""

    def __init__(self, publicador: "PublicadorSSE", execucao_id: str) -> None:
        self.execucao_id = execucao_id
        self.fila: asyncio.Queue[EventoSSE | None] = asyncio.Queue(maxsize=TAMANHO_FILA)
        self.encerrada = False
        self._publicador = publicador
        # o loop é capturado aqui, e não na publicação: quem publica é a thread do
        # threadpool da ingestão, que não tem loop próprio para descobrir
        self._loop = asyncio.get_running_loop()

    def enfileirar(self, evento: EventoSSE) -> None:
        """Chamada de qualquer thread. Só agenda; a fila é mexida dentro do loop."""
        if self.encerrada:
            return
        try:
            self._loop.call_soon_threadsafe(self._entregar, evento)
        except RuntimeError:
            # loop já fechado: este cliente não existe mais
            self.fechar()

    def _entregar(self, evento: EventoSSE) -> None:
        """Roda na thread do loop: só aqui é seguro mexer na fila."""
        if self.encerrada:
            return
        try:
            self.fila.put_nowait(evento)
        except asyncio.QueueFull:
            logger.warning(
                "fila cheia na execução %s: encerrando o fluxo do cliente para que ele "
                "reconecte e recupere o trajeto pelo Last-Event-ID",
                self.execucao_id,
            )
            self.encerrar()

    def encerrar(self) -> None:
        """Pede o fim do fluxo deste cliente. Roda na thread do loop."""
        self.fechar()
        try:
            self.fila.put_nowait(FIM)
        except asyncio.QueueFull:
            # a sentinela importa mais do que o evento mais antigo da fila
            self.fila.get_nowait()
            self.fila.put_nowait(FIM)

    def fechar(self) -> None:
        """Sai do registro sem mexer na fila. Chamada de qualquer thread."""
        self.encerrada = True
        self._publicador.desassinar(self)


class PublicadorSSE:
    """Registro das assinaturas por execução, com entrega a todos os clientes."""

    def __init__(self) -> None:
        self._assinaturas: dict[str, set[Assinatura]] = {}
        self._trava = threading.Lock()

    def assinar(self, execucao_id: str) -> Assinatura:
        """Exige um event loop rodando: é quem vai consumir a fila do cliente."""
        assinatura = Assinatura(self, execucao_id)
        with self._trava:
            self._assinaturas.setdefault(execucao_id, set()).add(assinatura)
        return assinatura

    def desassinar(self, assinatura: Assinatura) -> None:
        with self._trava:
            clientes = self._assinaturas.get(assinatura.execucao_id)
            if clientes is None:
                return
            clientes.discard(assinatura)
            if not clientes:
                # sem assinante a chave sai do dicionário, senão ele cresce sem fim
                del self._assinaturas[assinatura.execucao_id]

    def publicar(self, execucao_id: str, eventos: Sequence[EventoSSE]) -> None:
        """Entrega aos clientes da execução. Nunca levanta: o COMMIT já aconteceu."""
        if not eventos:
            return
        try:
            with self._trava:
                clientes = list(self._assinaturas.get(execucao_id, ()))
            for assinatura in clientes:
                for evento in eventos:
                    assinatura.enfileirar(evento)
        except Exception:
            logger.exception("falha ao publicar eventos da execução %s", execucao_id)

    def assinantes(self, execucao_id: str) -> int:
        with self._trava:
            return len(self._assinaturas.get(execucao_id, ()))

    def execucoes_ativas(self) -> int:
        with self._trava:
            return len(self._assinaturas)
