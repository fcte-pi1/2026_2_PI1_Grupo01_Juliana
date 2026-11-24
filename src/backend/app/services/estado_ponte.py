"""Estado do enlace da ponte com o robô e pedido de reconexão feito pela tela."""

import threading
from datetime import datetime
from typing import Literal

from app.services.monitor_conexao import Relogio, relogio_do_sistema

Enlace = Literal["conectada", "reconectando", "desligada"]

VALIDADE_DO_RELATO_S = 15


class EstadoPonte:
    """Guarda o último relato da ponte. Sem relato recente, a ponte conta como desligada."""

    def __init__(self, relogio: Relogio = relogio_do_sistema) -> None:
        self._relogio = relogio
        self._trava = threading.Lock()
        self._relato: tuple[Literal["conectada", "reconectando"], str, datetime] | None = None
        self._reconexao_pedida = False

    def registrar(self, estado: Literal["conectada", "reconectando"], porta: str) -> bool:
        """Anota o relato e diz se a tela pediu reconexão; o pedido vale uma vez só."""
        with self._trava:
            self._relato = (estado, porta, self._relogio())
            pedida, self._reconexao_pedida = self._reconexao_pedida, False
            return pedida

    def situacao(self) -> tuple[Enlace, str | None]:
        with self._trava:
            if self._relato is None:
                return "desligada", None
            estado, porta, quando = self._relato
            if (self._relogio() - quando).total_seconds() > VALIDADE_DO_RELATO_S:
                return "desligada", porta
            return estado, porta

    def pedir_reconexao(self) -> bool:
        """Registra o pedido se a ponte está ligada; senão devolve False."""
        if self.situacao()[0] == "desligada":
            return False
        with self._trava:
            self._reconexao_pedida = True
        return True

    @property
    def reconexao_pedida(self) -> bool:
        with self._trava:
            return self._reconexao_pedida
