"""Consultas adicionais de execuções (BACK-06)."""

from uuid import UUID

from app.models import ExecucaoLogica
from app.repositories.repositorio import Repositorio


class ExecucaoRepository(Repositorio):
    def buscar_por_id(self, execucao_id: UUID) -> ExecucaoLogica | None:
        return self.buscar_execucao(execucao_id)
