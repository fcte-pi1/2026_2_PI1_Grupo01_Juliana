"""Recalculo parcial e final de metricas na ingestao e no gerenciador (BACK-04)."""

from __future__ import annotations

from datetime import datetime
from decimal import Decimal

from app.models import ExecucaoLogica, LeituraTelemetria, PassoTrajeto, Tentativa
from app.services.calculo_metricas import (
    consumo_bateria_mv,
    consumo_bateria_pct,
    contar_deslocamentos,
    tempo_decorrido_s,
    velocidade_media_por_celulas,
)
from sqlalchemy import select
from sqlalchemy.orm import Session


def recalcular_parciais(
    sessao: Session,
    tentativa: Tentativa,
    execucao: ExecucaoLogica,
    agora: datetime,
    *,
    bateria_mv_atual: int | Decimal | None = None,
) -> None:
    _atualizar_bateria(tentativa, sessao, bateria_mv_atual)
    _atualizar_tempo_e_velocidade(sessao, tentativa, execucao, agora)


def finalizar_tentativa(
    sessao: Session,
    tentativa: Tentativa,
    execucao: ExecucaoLogica,
    agora: datetime,
) -> None:
    _atualizar_bateria(tentativa, sessao, None)
    _atualizar_tempo_e_velocidade(sessao, tentativa, execucao, agora)
    if execucao.encerrada_em is not None:
        execucao.tempo_total_s = tempo_decorrido_s(execucao.iniciada_em, execucao.encerrada_em)


def _atualizar_bateria(
    tentativa: Tentativa,
    sessao: Session,
    bateria_mv_atual: int | Decimal | None,
) -> None:
    if bateria_mv_atual is not None:
        mv = Decimal(bateria_mv_atual)
    else:
        mv = _ultima_bateria_mv(sessao, tentativa.tentativa_id)
    if mv is None:
        return
    # Leitura inicial desta tentativa. O painel da execução não usa esta
    # de novo: o consumo geral parte da bateria_inicial da primeira.
    if tentativa.bateria_inicial is None:
        tentativa.bateria_inicial = mv
    tentativa.bateria_final = mv
    tentativa.consumo_bateria = consumo_bateria_mv(tentativa.bateria_inicial, mv)
    tentativa.consumo_bateria_pct = consumo_bateria_pct(tentativa.bateria_inicial, mv)


def _ultima_bateria_mv(sessao: Session, tentativa_id) -> Decimal | None:
    stmt = (
        select(LeituraTelemetria.bateria)
        .where(LeituraTelemetria.tentativa_id == tentativa_id)
        .order_by(LeituraTelemetria.ordem.desc())
        .limit(1)
    )
    return sessao.scalar(stmt)


def _coordenadas_passos(sessao: Session, tentativa_id) -> list[tuple[int, int]]:
    stmt = (
        select(PassoTrajeto.x, PassoTrajeto.y)
        .where(PassoTrajeto.tentativa_id == tentativa_id)
        .order_by(PassoTrajeto.seq)
    )
    return list(sessao.execute(stmt).all())


def _atualizar_tempo_e_velocidade(
    sessao: Session,
    tentativa: Tentativa,
    execucao: ExecucaoLogica,
    agora: datetime,
) -> None:
    fim_tentativa = tentativa.encerrada_em or agora
    tentativa.tempo_s = tempo_decorrido_s(tentativa.iniciada_em, fim_tentativa)

    deslocamentos = contar_deslocamentos(_coordenadas_passos(sessao, tentativa.tentativa_id))
    tentativa.velocidade_media = velocidade_media_por_celulas(deslocamentos, tentativa.tempo_s)

    if execucao.status == "em_andamento":
        execucao.tempo_total_s = tempo_decorrido_s(execucao.iniciada_em, agora)
