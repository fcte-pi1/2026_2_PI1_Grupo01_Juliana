"""Consultas de execuções e tentativas (BACK-01, BACK-06)."""

from uuid import UUID

from app.models import ExecucaoLogica
from sqlalchemy.orm import Session


class ExecucaoRepository:
    def __init__(self, session: Session) -> None:
        self._session = session

    def buscar_por_id(self, execucao_id: UUID) -> ExecucaoLogica | None:
        return self._session.get(ExecucaoLogica, execucao_id)
