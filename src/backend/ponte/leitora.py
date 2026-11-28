"""Leitora da serial: lê linha a linha, põe na fila e reabre a porta quando o link cai (D6)."""

import threading
import time
from collections.abc import Callable
from datetime import datetime
from typing import Protocol

import serial

from ponte.auditoria import Auditoria
from ponte.espera import Espera
from ponte.fila import Fila, classificar

ERROS_DA_PORTA = (serial.SerialException, OSError)

# O robô manda uma `tel` por segundo. No Windows, quando o Bluetooth cai (robô desligado ou reiniciado), a porta
# COM do SPP continua aberta e a leitura só devolve vazio, sem erro: sem este limite a ponte ficaria "conectada"
# para sempre, sem receber nada e sem entregar os comandos da tela. Passado o silêncio, a porta é reaberta.
SILENCIO_MAXIMO_S = 5.0


class PortaEmSilencio(Exception):
    """A porta está aberta, mas não chega nada há mais de `SILENCIO_MAXIMO_S`."""


class ReconexaoPedida(Exception):
    """A tela pediu para fechar e reabrir a porta."""


class Porta(Protocol):
    """O que a leitora usa de `serial.Serial`."""

    def readline(self) -> bytes: ...

    def write(self, dados: bytes) -> int | None: ...

    def close(self) -> None: ...


def abrir_serial(porta: str) -> Porta:
    """Abre a porta: um caminho (`/dev/rfcomm0`, `COM5`) ou uma URL do pyserial (`socket://ratatouille.local:3333`,
    o enlace Wi-Fi de testes do firmware `esp32_real_wifi`)."""
    return serial.serial_for_url(porta, 115200, timeout=0.2)


def _agora() -> datetime:
    return datetime.now().astimezone()


class Leitora:
    """Lê a serial numa thread e expõe `escrever()` para a enviadora, com trava de escrita."""

    def __init__(
        self,
        porta: str,
        fila: Fila,
        auditoria: Auditoria,
        abrir: Callable[[str], Porta] = abrir_serial,
        relogio: Callable[[], datetime] = _agora,
        dormir: Callable[[float], None] = time.sleep,
        tique: Callable[[], float] = time.monotonic,
        silencio_maximo_s: float = SILENCIO_MAXIMO_S,
    ) -> None:
        self._nome = porta
        self._fila = fila
        self._auditoria = auditoria
        self._abrir = abrir
        self._relogio = relogio
        self._dormir = dormir
        self._tique = tique
        self._silencio_maximo_s = silencio_maximo_s
        self._espera = Espera()
        self._trava = threading.Lock()
        self._porta: Porta | None = None
        self._reabrir_ja = threading.Event()

    @property
    def aberta(self) -> bool:
        with self._trava:
            return self._porta is not None

    def reconectar(self) -> None:
        """Pede para fechar a porta e reabrir sem esperar. Quem fecha é a thread da leitura, na volta da
        leitura em curso (até 0,2 s): no Windows, fechar a porta do Bluetooth de outra thread, com uma leitura
        pendente, deixou a porta presa e a reabertura falhou com "Acesso negado" até reiniciar a ponte."""
        self._reabrir_ja.set()
        self._auditoria.registrar("CONEXAO", "reconexao pedida pela tela")

    def rodar(self, parar: threading.Event) -> None:
        """Abre, lê e reabre a porta até `parar` ser sinalizado."""
        while not parar.is_set():
            try:
                porta = self._abrir(self._nome)
            except Exception as erro:
                self._caiu(erro, parar)
                continue
            with self._trava:
                self._porta = porta
            self._auditoria.registrar("CONEXAO", f"aberta {self._nome}")
            self._espera.zerar()
            try:
                self._ler(porta, parar)
            except Exception as erro:
                self._caiu(erro, parar)
        self._fechar()

    def escrever(self, linha: str) -> bool:
        """Escreve a linha na porta aberta e diz se escreveu; senão, registra o comando perdido."""
        comando = linha.rstrip("\n")
        with self._trava:
            if self._porta is None:
                self._auditoria.registrar("CONEXAO", f"sem porta, comando perdido {comando}")
                return False
            try:
                self._porta.write(linha.encode("utf-8"))
            except ERROS_DA_PORTA as erro:
                self._auditoria.registrar(
                    "CONEXAO", f"erro na escrita {erro}, comando perdido {comando}"
                )
                return False
            return True

    def _ler(self, porta: Porta, parar: threading.Event) -> None:
        pedacos = b""
        primeira = True
        ultimo_dado = self._tique()
        while not parar.is_set():
            lido = porta.readline()
            if self._reabrir_ja.is_set():
                raise ReconexaoPedida()
            if lido:
                ultimo_dado = self._tique()
            elif self._tique() - ultimo_dado > self._silencio_maximo_s:
                raise PortaEmSilencio(f"sem dados ha mais de {self._silencio_maximo_s:g} s")
            pedacos += lido
            while b"\n" in pedacos:
                bruta, pedacos = pedacos.split(b"\n", 1)
                recebido_em = self._relogio()
                if primeira:
                    # A primeira linha depois de abrir pode estar pela metade.
                    primeira = False
                    continue
                self._entregar(bruta, recebido_em)

    def _entregar(self, bruta: bytes, recebido_em: datetime) -> None:
        if bruta.endswith(b"\r"):
            bruta = bruta[:-1]
        if not bruta:
            return
        linha = bruta.decode("utf-8", errors="replace")
        self._auditoria.registrar("RX", linha)
        descartado = self._fila.colocar((linha, recebido_em))
        if descartado is not None:
            self._auditoria.registrar(
                "FILA", f"descartada {classificar(descartado[0])} {descartado[0]}"
            )

    def _caiu(self, erro: BaseException, parar: threading.Event) -> None:
        pedida = self._reabrir_ja.is_set()
        self._reabrir_ja.clear()
        if not pedida:
            self._auditoria.registrar("CONEXAO", f"caiu {erro}")
        self._fechar()
        if pedida:
            self._espera.zerar()
        elif not parar.is_set():
            self._dormir(self._espera.proxima())

    def _fechar(self) -> None:
        with self._trava:
            porta, self._porta = self._porta, None
        if porta is not None:
            try:
                porta.close()
            except ERROS_DA_PORTA:
                pass
