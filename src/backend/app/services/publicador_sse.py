"""Fila de eventos SSE por cliente (BACK-07)."""

import asyncio
from collections import defaultdict
from typing import Any


class PublicadorSSE:
    def __init__(self) -> None:
        self._filas: dict[str, asyncio.Queue[dict[str, Any]]] = defaultdict(asyncio.Queue)

    def fila(self, execucao_id: str) -> asyncio.Queue[dict[str, Any]]:
        return self._filas[execucao_id]
