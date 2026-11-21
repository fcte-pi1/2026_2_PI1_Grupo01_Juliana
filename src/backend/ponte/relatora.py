"""Relatora: informa a API o estado do enlace e recebe o pedido de reconexão da tela."""

import threading
from collections.abc import Callable

import httpx

from ponte.auditoria import Auditoria

INTERVALO_DO_RELATO_S = 3.0


class Relatora:
    """POST /robo/ponte com `conectada` ou `reconectando`; se a tela pediu, reabre a porta."""

    def __init__(
        self,
        cliente: httpx.Client,
        porta: str,
        aberta: Callable[[], bool],
        reconectar: Callable[[], None],
        auditoria: Auditoria,
        dormir: Callable[[float], None],
    ) -> None:
        self._cliente = cliente
        self._porta = porta
        self._aberta = aberta
        self._reconectar = reconectar
        self._auditoria = auditoria
        self._dormir = dormir
        self._api_fora = False

    def relatar(self) -> None:
        corpo = {"estado": "conectada" if self._aberta() else "reconectando", "porta": self._porta}
        try:
            resposta = self._cliente.post("/robo/ponte", json=corpo)
            pedido = resposta.status_code == 200 and bool(resposta.json().get("reconectar"))
        except (httpx.TransportError, ValueError) as erro:
            if not self._api_fora:
                self._auditoria.registrar("API", f"relato do enlace falhou {type(erro).__name__}")
            self._api_fora = True
            return
        self._api_fora = False
        if pedido:
            self._reconectar()

    def rodar(self, parar: threading.Event) -> None:
        while not parar.is_set():
            self.relatar()
            self._dormir(INTERVALO_DO_RELATO_S)
