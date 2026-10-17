"""Publicação tolerante a falhas de eventos SSE depois do COMMIT."""

from __future__ import annotations

import logging
import uuid

from app.services.publicador_sse import EventoSSE, PublicadorSSE

logger = logging.getLogger("app.sse")


def publicar_apos_commit(
    publicador: PublicadorSSE | None,
    execucao_id: uuid.UUID | str,
    eventos: list[EventoSSE],
) -> None:
    """Publica sem permitir que uma falha do cliente desfaça o estado persistido."""
    if publicador is None or not eventos:
        return
    try:
        publicador.publicar(str(execucao_id), eventos)
    except Exception:
        logger.exception("eventos não publicados na execução %s", execucao_id)
