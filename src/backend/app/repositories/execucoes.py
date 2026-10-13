"""Consultas adicionais de execuções (BACK-06)."""

from datetime import datetime
from decimal import Decimal
from uuid import UUID

from app.models import (
    ExecucaoLogica,
    Labirinto,
    LeituraTelemetria,
    Tentativa,
)
from app.repositories.filtros import FiltroExecucoes
from app.repositories.repositorio import Repositorio
from sqlalchemy import case, func, select
from sqlalchemy.orm import joinedload, selectinload


class ExecucaoRepository(Repositorio):
    def buscar_por_id(self, execucao_id: UUID) -> ExecucaoLogica | None:
        return self.buscar_execucao(execucao_id)

    def listar_pagina(
        self,
        filtro: FiltroExecucoes | None = None,
        *,
        limite: int,
        offset: int = 0,
    ) -> tuple[list[ExecucaoLogica], int]:
        """Página do histórico: em andamento primeiro, depois da mais recente à mais antiga."""
        filtro = filtro or FiltroExecucoes()
        base = select(ExecucaoLogica)
        if filtro.tipo_labirinto is not None:
            base = base.join(Labirinto).where(Labirinto.tipo == filtro.tipo_labirinto)

        total = self._session.scalar(select(func.count()).select_from(base.subquery())) or 0

        em_andamento_primeiro = case((ExecucaoLogica.status == "em_andamento", 0), else_=1)
        stmt = (
            base.options(
                joinedload(ExecucaoLogica.labirinto),
                selectinload(ExecucaoLogica.tentativas),
            )
            .order_by(
                em_andamento_primeiro,
                ExecucaoLogica.iniciada_em.desc(),
                ExecucaoLogica.execucao_id,
            )
            .limit(limite)
            .offset(offset)
        )
        return list(self._session.scalars(stmt).unique().all()), total

    def buscar_detalhe(self, execucao_id: UUID) -> ExecucaoLogica | None:
        """Execução com tentativas, health-check, falha, trajeto e paredes já carregados."""
        stmt = (
            select(ExecucaoLogica)
            .where(ExecucaoLogica.execucao_id == execucao_id)
            .options(
                joinedload(ExecucaoLogica.labirinto),
                selectinload(ExecucaoLogica.tentativas).selectinload(Tentativa.health_check_itens),
                selectinload(ExecucaoLogica.tentativas).selectinload(Tentativa.falha),
                selectinload(ExecucaoLogica.passos_trajeto),
                selectinload(ExecucaoLogica.paredes),
            )
        )
        execucao = self._session.scalar(stmt)
        if execucao is not None:
            execucao.tentativas.sort(key=lambda t: t.attempt_index)
            execucao.passos_trajeto.sort(key=lambda p: p.seq)
        return execucao

    def listar_leituras(self, tentativa_id: UUID) -> list[LeituraTelemetria]:
        stmt = (
            select(LeituraTelemetria)
            .where(LeituraTelemetria.tentativa_id == tentativa_id)
            .order_by(LeituraTelemetria.ordem)
        )
        return list(self._session.scalars(stmt).all())

    def ultima_mensagem_em(self, execucao_id: UUID) -> datetime | None:
        stmt = (
            select(func.max(LeituraTelemetria.recebido_em))
            .join(Tentativa, LeituraTelemetria.tentativa_id == Tentativa.tentativa_id)
            .where(Tentativa.execucao_id == execucao_id)
        )
        return self._session.scalar(stmt)

    def melhor_tempo_anterior(self, execucao: ExecucaoLogica) -> Decimal | None:
        """Melhor tempo de sucesso no mesmo labirinto entre as execuções iniciadas antes desta."""
        stmt = (
            select(func.min(Tentativa.tempo_s))
            .join(ExecucaoLogica, Tentativa.execucao_id == ExecucaoLogica.execucao_id)
            .where(ExecucaoLogica.labirinto_id == execucao.labirinto_id)
            .where(ExecucaoLogica.execucao_id != execucao.execucao_id)
            .where(ExecucaoLogica.iniciada_em < execucao.iniciada_em)
            .where(Tentativa.status == "success")
            .where(Tentativa.tempo_s.is_not(None))
        )
        return self._session.scalar(stmt)
