"""Persistência de leituras e passos de trajeto (BACK-06)."""

from uuid import UUID

from app.models import LeituraTelemetria
from sqlalchemy import func, select
from sqlalchemy.orm import Session


class TelemetriaRepository:
    def __init__(self, session: Session) -> None:
        self._session = session

    def contar_leituras(self) -> int:
        stmt = select(func.count()).select_from(LeituraTelemetria)
        return int(self._session.scalar(stmt) or 0)

    def proxima_ordem_leitura(self, tentativa_id: UUID) -> int:
        stmt = select(func.coalesce(func.max(LeituraTelemetria.ordem), 0)).where(
            LeituraTelemetria.tentativa_id == tentativa_id
        )
        return int(self._session.scalar(stmt)) + 1
