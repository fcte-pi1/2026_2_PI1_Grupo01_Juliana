"""Emissor: leva a linha do tempo do gerador para o tempo real.

Dorme até o instante de cada item (dividido por --acelerar), aplica o limitador de
20 linhas por segundo e entrega o texto a uma saída. O relógio é injetável, para os
testes não precisarem esperar.
"""

import logging
import time
from collections import deque
from collections.abc import Callable, Iterable
from typing import Protocol

from contrato.telemetria import Mensagem, escrever_linha

from simulador.gerador import Item, LinhaCrua

LINHAS_POR_SEGUNDO = 20  # teto do enlace (FR-3)
TAXA_TEL_MINIMA_HZ = 1
TAXA_TEL_MAXIMA_HZ = 20

log = logging.getLogger("simulador")


class Relogio(Protocol):
    """Fonte de tempo do emissor, em segundos."""

    def agora(self) -> float: ...

    def dormir(self, segundos: float) -> None: ...


class RelogioReal:
    """Relógio monotônico do sistema."""

    def agora(self) -> float:
        return time.monotonic()

    def dormir(self, segundos: float) -> None:
        time.sleep(segundos)


class Limitador:
    """Janela deslizante: no máximo `limite` linhas em qualquer segundo de tempo real."""

    def __init__(self, relogio: Relogio, limite: int = LINHAS_POR_SEGUNDO):
        self.relogio = relogio
        self.limite = limite
        self._envios: deque[float] = deque(maxlen=limite)

    def aguardar(self) -> None:
        """Dorme o necessário para a próxima linha caber na janela e registra o envio."""
        if len(self._envios) == self.limite:
            espera = self._envios[0] + 1 - self.relogio.agora()
            if espera > 0:
                self.relogio.dormir(espera)
        self._envios.append(self.relogio.agora())


def texto(item: Item, acelerar: float = 1) -> str:
    """Linha a enviar: a mensagem com o t_ms dividido por `acelerar`, ou o texto cru."""
    if isinstance(item, LinhaCrua):
        return item.linha if item.linha.endswith("\n") else item.linha + "\n"
    mensagem: Mensagem = item
    if acelerar != 1:
        mensagem = mensagem.model_copy(update={"t_ms": int(mensagem.t_ms / acelerar)})
    return escrever_linha(mensagem)


def emitir(
    itens: Iterable[tuple[int, Item]],
    escrever: Callable[[str], None],
    relogio: Relogio | None = None,
    acelerar: float = 1,
    sem_espera: bool = False,
) -> int:
    """Envia cada item no seu instante (t simulado ÷ acelerar); devolve quantas linhas saíram.

    Com `sem_espera`, emite tudo de uma vez, sem relógio nem limitador.
    """
    relogio = relogio or RelogioReal()
    limitador = Limitador(relogio)
    inicio = relogio.agora()
    enviadas = 0
    for t_simulado_ms, item in itens:
        if not sem_espera:
            espera = inicio + t_simulado_ms / 1000 / acelerar - relogio.agora()
            if espera > 0:
                relogio.dormir(espera)
            limitador.aguardar()
        linha = texto(item, acelerar)
        escrever(linha)
        enviadas += 1
        log.info("enviada: %s", linha.rstrip("\n"))
    return enviadas
