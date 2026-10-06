"""Saídas do simulador que não são o stdout.

`SaidaPty` abre uma porta serial virtual (pty) e faz o papel do SPP do robô: a ponte (#131)
abre o lado escravo, recebe as linhas escritas no lado mestre e pode mandar comandos de
volta, que chegam linha a linha a um callback.
"""

import logging
import os
import select
import sys
import threading
import time
from collections.abc import Callable
from typing import Self

log = logging.getLogger("simulador")

_INTERVALO_S = 0.1  # de quanto em quanto tempo as esperas conferem se devem parar


class PtyIndisponivel(RuntimeError):
    """O sistema não tem pty (Windows nativo)."""


class SaidaPty:
    """Porta serial virtual: escreve no lado mestre e lê os comandos do outro lado."""

    def __init__(self, ao_receber: Callable[[str], None] | None = None):
        if sys.platform == "win32" or not hasattr(os, "openpty"):
            raise PtyIndisponivel(
                "--saida pty precisa de Linux, macOS ou WSL; no Windows nativo use "
                "--saida stdout ou --saida http"
            )
        import tty  # só existe em sistemas POSIX

        self.ao_receber = ao_receber or (lambda linha: None)
        self.mestre, escravo = os.openpty()
        # Modo cru: sem eco (as linhas voltariam como comandos) e sem trocar \n por \r\n.
        tty.setraw(escravo)
        self.caminho = os.ttyname(escravo)
        # Sem o escravo aberto aqui, o mestre fica em POLLHUP até a ponte abrir a porta.
        os.close(escravo)
        self._parar = threading.Event()
        self._leitor: threading.Thread | None = None

    def aguardar_conexao(self, timeout_s: float | None = None) -> bool:
        """Espera alguém abrir o lado escravo e começa a ler os comandos.

        Devolve False se `timeout_s` passar antes disso.
        """
        poll = select.poll()
        poll.register(self.mestre, select.POLLIN)
        restante = timeout_s
        while any(evento & select.POLLHUP for _, evento in poll.poll(_INTERVALO_S * 1000)):
            if restante is not None:
                restante -= _INTERVALO_S
                if restante <= 0:
                    return False
        self._leitor = threading.Thread(target=self._ler, name="pty-leitor", daemon=True)
        self._leitor.start()
        return True

    def escrever(self, linha: str) -> None:
        """Escreve a linha inteira no lado mestre."""
        dados = linha.encode()
        while dados:
            dados = dados[os.write(self.mestre, dados) :]

    def _ler(self) -> None:
        """Lê o que chega do outro lado e entrega cada linha completa ao callback."""
        resto = b""
        while not self._parar.is_set():
            prontos, _, _ = select.select([self.mestre], [], [], _INTERVALO_S)
            if not prontos:
                continue
            try:
                dados = os.read(self.mestre, 1024)
            except OSError:  # EIO: o outro lado fechou a porta
                log.info("a porta %s foi fechada do outro lado", self.caminho)
                return
            if not dados:
                return
            *linhas, resto = (resto + dados).split(b"\n")
            for linha in linhas:
                self.ao_receber(linha.rstrip(b"\r").decode(errors="replace"))

    def esvaziar(self, timeout_s: float = 2.0) -> None:
        """Espera o outro lado ler o que já foi escrito (até `timeout_s`).

        Fechar o mestre descarta o que o escravo ainda não leu; por isso `fechar` chama isto
        antes. A fila é medida com FIONREAD num descritor próprio do lado escravo; como o
        kernel passa os bytes do mestre para o escravo de forma assíncrona, a fila precisa
        aparecer vazia em leituras seguidas por `_INTERVALO_S`.
        """
        import fcntl
        import termios

        try:
            escravo = os.open(self.caminho, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        except OSError:
            return
        try:
            limite = time.monotonic() + timeout_s
            pendente = bytearray(4)
            vazia_desde: float | None = None
            while (agora := time.monotonic()) < limite:
                fcntl.ioctl(escravo, termios.FIONREAD, pendente)
                if int.from_bytes(pendente, sys.byteorder) > 0:
                    vazia_desde = None
                elif vazia_desde is None:
                    vazia_desde = agora
                elif agora - vazia_desde >= _INTERVALO_S:
                    return
                time.sleep(_INTERVALO_S / 10)
            log.warning("a ponte não leu tudo em %g s; o resto se perde", timeout_s)
        finally:
            os.close(escravo)

    def fechar(self) -> None:
        """Para o leitor e fecha o lado mestre (o outro lado recebe fim de arquivo)."""
        if self._leitor is not None and self._leitor.is_alive():
            self.esvaziar()
        self._parar.set()
        if self._leitor is not None:
            self._leitor.join()
        os.close(self.mestre)

    def __enter__(self) -> Self:
        return self

    def __exit__(self, *_: object) -> None:
        self.fechar()
