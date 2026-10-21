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


class Porta(Protocol):
    """O que a leitora usa de `serial.Serial`."""

    def readline(self) -> bytes: ...

    def write(self, dados: bytes) -> int | None: ...

    def close(self) -> None: ...


def abrir_serial(porta: str) -> Porta:
    return serial.Serial(porta, 115200, timeout=0.2)


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
    ) -> None:
        self._nome = porta
        self._fila = fila
        self._auditoria = auditoria
        self._abrir = abrir
        self._relogio = relogio
        self._dormir = dormir
        self._espera = Espera()
        self._trava = threading.Lock()
        self._porta: Porta | None = None

    def rodar(self, parar: threading.Event) -> None:
        """Abre, lê e reabre a porta até `parar` ser sinalizado."""
        while not parar.is_set():
            try:
                porta = self._abrir(self._nome)
            except ERROS_DA_PORTA as erro:
                self._caiu(erro, parar)
                continue
            with self._trava:
                self._porta = porta
            self._auditoria.registrar("CONEXAO", f"aberta {self._nome}")
            self._espera.zerar()
            try:
                self._ler(porta, parar)
            except ERROS_DA_PORTA as erro:
                self._caiu(erro, parar)
        self._fechar()

    def escrever(self, linha: str) -> None:
        """Escreve a linha na porta aberta; sem porta, registra o comando perdido."""
        comando = linha.rstrip("\n")
        with self._trava:
            if self._porta is None:
                self._auditoria.registrar("CONEXAO", f"sem porta, comando perdido {comando}")
                return
            try:
                self._porta.write(linha.encode("utf-8"))
            except ERROS_DA_PORTA as erro:
                self._auditoria.registrar(
                    "CONEXAO", f"erro na escrita {erro}, comando perdido {comando}"
                )

    def _ler(self, porta: Porta, parar: threading.Event) -> None:
        pedacos = b""
        primeira = True
        while not parar.is_set():
            pedacos += porta.readline()
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
        self._auditoria.registrar("CONEXAO", f"caiu {erro}")
        self._fechar()
        if not parar.is_set():
            self._dormir(self._espera.proxima())

    def _fechar(self) -> None:
        with self._trava:
            porta, self._porta = self._porta, None
        if porta is not None:
            try:
                porta.close()
            except ERROS_DA_PORTA:
                pass
