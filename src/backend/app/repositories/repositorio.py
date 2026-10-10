"""Persistência e consultas do domínio (BACK-01)."""

from decimal import Decimal
from uuid import UUID

from app.db import Base
from app.models import ExecucaoLogica, Labirinto, Tentativa
from app.repositories.filtros import FiltroExecucoes
from sqlalchemy import func, select
from sqlalchemy.orm import Session, joinedload, selectinload


class Repositorio:
    def __init__(self, session: Session) -> None:
        self._session = session

    def salvar(self, entidade: Base) -> None:
        self._session.add(entidade)
        self._session.flush()

    def buscar_execucao(self, execucao_id: UUID) -> ExecucaoLogica | None:
        stmt = (
            select(ExecucaoLogica)
            .where(ExecucaoLogica.execucao_id == execucao_id)
            .options(
                joinedload(ExecucaoLogica.labirinto),
                selectinload(ExecucaoLogica.tentativas),
            )
        )
        execucao = self._session.scalar(stmt)
        if execucao is not None:
            execucao.tentativas.sort(key=lambda t: t.attempt_index)
        return execucao

    def listar_execucoes(self, filtro: FiltroExecucoes | None = None) -> list[ExecucaoLogica]:
        filtro = filtro or FiltroExecucoes()
        stmt = select(ExecucaoLogica).options(joinedload(ExecucaoLogica.labirinto))
        if filtro.tipo_labirinto is not None:
            stmt = stmt.join(Labirinto).where(Labirinto.tipo == filtro.tipo_labirinto)
        stmt = stmt.order_by(ExecucaoLogica.iniciada_em.desc())
        return list(self._session.scalars(stmt).unique().all())

    def melhor_tempo(self, tipo_labirinto: str) -> Decimal | None:
        stmt = (
            select(func.min(Tentativa.tempo_s))
            .select_from(Tentativa)
            .join(ExecucaoLogica, Tentativa.execucao_id == ExecucaoLogica.execucao_id)
            .join(Labirinto, ExecucaoLogica.labirinto_id == Labirinto.labirinto_id)
            .where(Labirinto.tipo == tipo_labirinto)
            .where(Tentativa.status == "success")
            .where(Tentativa.tempo_s.is_not(None))
        )
        return self._session.scalar(stmt)

    def buscar_tentativa_aberta(self) -> Tentativa | None:
        """Tentativa em health-check ou running; se houver mais de uma, a mais recente."""
        stmt = (
            select(Tentativa)
            .where(Tentativa.status.in_(("health-check", "running")))
            .options(joinedload(Tentativa.execucao).joinedload(ExecucaoLogica.labirinto))
            .order_by(Tentativa.iniciada_em.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)
