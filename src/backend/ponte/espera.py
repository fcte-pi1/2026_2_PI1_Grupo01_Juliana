"""Espera crescente entre tentativas de reconexão (D5)."""

_PASSOS: tuple[float, ...] = (0.5, 1.0, 2.0)


class Espera:
    """Devolve 0,5 s, 1 s, 2 s e depois fica em 2 s; `zerar()` volta ao início."""

    def __init__(self) -> None:
        self._indice = 0

    def proxima(self) -> float:
        valor = _PASSOS[min(self._indice, len(_PASSOS) - 1)]
        self._indice += 1
        return valor

    def zerar(self) -> None:
        self._indice = 0
