"""Persistência e consultas do domínio (BACK-01)."""

from decimal import Decimal
from uuid import UUID

from app.db import Base
from app.models import ExecucaoLogica, HealthCheck, Labirinto, Tentativa
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
        """Tentativa em running; se houver mais de uma, a mais recente."""
        stmt = (
            select(Tentativa)
            .where(Tentativa.status == "running")
            .options(joinedload(Tentativa.execucao).joinedload(ExecucaoLogica.labirinto))
            .order_by(Tentativa.iniciada_em.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)

    def buscar_tentativa_aberta_da_execucao(self, execucao_id: UUID) -> Tentativa | None:
        stmt = (
            select(Tentativa)
            .where(Tentativa.execucao_id == execucao_id)
            .where(Tentativa.status == "running")
            .limit(1)
        )
        return self._session.scalar(stmt)

    def ultimo_health_check(self) -> HealthCheck | None:
        """O health-check mais recente, concluído ou ainda recebendo itens."""
        stmt = (
            select(HealthCheck)
            .options(selectinload(HealthCheck.itens))
            .order_by(HealthCheck.iniciado_em.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)

    def ultimo_health_check_concluido(self) -> HealthCheck | None:
        stmt = (
            select(HealthCheck)
            .where(HealthCheck.concluido_em.is_not(None))
            .order_by(HealthCheck.concluido_em.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)

    def health_check_em_andamento(self, boot: int) -> HealthCheck | None:
        stmt = (
            select(HealthCheck)
            .where(HealthCheck.boot == boot)
            .where(HealthCheck.concluido_em.is_(None))
            .order_by(HealthCheck.iniciado_em.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)

    def buscar_execucao_em_andamento(self) -> ExecucaoLogica | None:
        stmt = (
            select(ExecucaoLogica)
            .where(ExecucaoLogica.status == "em_andamento")
            .options(joinedload(ExecucaoLogica.labirinto))
            .limit(1)
        )
        return self._session.scalar(stmt)

    def bloquear_execucao(self, execucao_id: UUID) -> ExecucaoLogica | None:
        stmt = (
            select(ExecucaoLogica)
            .where(ExecucaoLogica.execucao_id == execucao_id)
            .options(
                joinedload(ExecucaoLogica.labirinto),
                selectinload(ExecucaoLogica.tentativas),
            )
            .with_for_update(of=ExecucaoLogica)
        )
        execucao = self._session.scalar(stmt)
        if execucao is not None:
            execucao.tentativas.sort(key=lambda t: t.attempt_index)
        return execucao

    def labirinto_id_por_tipo(self, tipo: str) -> int | None:
        return self._session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == tipo))
