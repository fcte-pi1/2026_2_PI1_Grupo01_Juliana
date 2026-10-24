"""Fila da ponte entre a leitora e a enviadora, com descarte por prioridade (D3)."""

import json
import threading
from collections import deque
from datetime import datetime
from typing import Literal

Item = tuple[str, datetime]
Classe = Literal["invalida", "tel", "evento"]

# Ordem de descarte com a fila cheia: o que menos importa sai primeiro.
_ORDEM_DE_DESCARTE: tuple[Classe, ...] = ("invalida", "tel", "evento")


def classificar(linha: str) -> Classe:
    """Classifica a linha só pelo `tipo`, sem alterá-la."""
    try:
        objeto = json.loads(linha)
    except ValueError:
        return "invalida"
    if not isinstance(objeto, dict) or not isinstance(objeto.get("tipo"), str):
        return "invalida"
    return "tel" if objeto["tipo"] == "tel" else "evento"


class _Entrada:
    """Guarda o item e a classe; a identidade separa linhas iguais."""

    __slots__ = ("item", "classe")

    def __init__(self, item: Item) -> None:
        self.item = item
        self.classe = classificar(item[0])


class Fila:
    """Fila de no máximo `maximo` itens `(linha, recebido_em)`, segura entre threads.

    A enviadora lê com `primeiro()` e só remove com `remover_primeiro()` depois do envio,
    então a ordem de envio é sempre a de chegada.
    """

    def __init__(self, maximo: int) -> None:
        if maximo < 1:
            raise ValueError("maximo precisa ser pelo menos 1")
        self._maximo = maximo
        self._entradas: deque[_Entrada] = deque()
        self._entregue: _Entrada | None = None
        self._condicao = threading.Condition()

    def __len__(self) -> int:
        with self._condicao:
            return len(self._entradas)

    def colocar(self, item: Item) -> Item | None:
        """Põe o item no fim; com a fila cheia, descarta um e o devolve."""
        with self._condicao:
            descartado = None
            if len(self._entradas) >= self._maximo:
                descartado = self._descartar()
            self._entradas.append(_Entrada(item))
            self._condicao.notify_all()
            return descartado

    def primeiro(self, timeout: float | None = None) -> Item | None:
        """Devolve o item mais antigo sem removê-lo, ou None se o timeout estourar."""
        with self._condicao:
            if not self._condicao.wait_for(lambda: self._entradas, timeout):
                return None
            self._entregue = self._entradas[0]
            return self._entregue.item

    def remover_primeiro(self) -> bool:
        """Remove o item devolvido por `primeiro()`, se ele ainda estiver no início."""
        with self._condicao:
            if self._entregue is None or not self._entradas:
                return False
            if self._entradas[0] is not self._entregue:
                return False
            self._entradas.popleft()
            self._entregue = None
            return True

    def _descartar(self) -> Item:
        for classe in _ORDEM_DE_DESCARTE:
            for entrada in self._entradas:
                if entrada.classe == classe:
                    self._entradas.remove(entrada)
                    if entrada is self._entregue:
                        self._entregue = None
                    return entrada.item
        raise AssertionError("fila cheia sem entradas")
