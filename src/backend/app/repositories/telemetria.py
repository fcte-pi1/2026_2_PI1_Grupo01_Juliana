"""Persistência de leituras e passos de trajeto (BACK-06)."""

from app.models import LeituraTelemetria
from sqlalchemy import func, select
from sqlalchemy.orm import Session


class TelemetriaRepository:
    def __init__(self, session: Session) -> None:
        self._session = session

    def contar_leituras(self) -> int:
        stmt = select(func.count()).select_from(LeituraTelemetria)
        return int(self._session.scalar(stmt) or 0)
