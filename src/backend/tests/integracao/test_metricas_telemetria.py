"""Metricas parciais na ingestao e finais no encerramento (BACK-04)."""

from __future__ import annotations

import json
import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

from app.models import ExecucaoLogica, Tentativa
from contrato.telemetria import HcResultado, Sucesso, escrever_linha
from sqlalchemy import select
from sqlalchemy.orm import Session

def _t0() -> datetime:
    """Inicio recente para nao estourar RF32 (10 min) no verificar_monitores."""
    return datetime.now(UTC) - timedelta(seconds=60)


def _iso(dt: datetime) -> str:
    return dt.isoformat().replace("+00:00", "Z")


def _telemetria(client, linha: str, recebido_em: datetime):
    return client.post(
        "/telemetria",
        json={"linha": linha.strip(), "recebido_em": _iso(recebido_em)},
    )


def _evento(seq: int, campos: dict) -> str:
    return json.dumps({"v": 1, "boot": 9, "seq": seq, "t_ms": 1000 + seq * 100} | campos)


def _alinhar_inicio(db_session: Session, execucao_id: str) -> tuple[Tentativa, datetime]:
    inicio = _t0()
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
    tentativa = db_session.scalar(
        select(Tentativa).where(Tentativa.execucao_id == execucao.execucao_id)
    )
    execucao.iniciada_em = inicio
    tentativa.iniciada_em = inicio
    db_session.commit()
    return tentativa, inicio


def _hc_aprovado(seq: int = 1) -> str:
    return escrever_linha(
        HcResultado(
            v=1,
            boot=9,
            seq=seq,
            t_ms=100,
            tipo="hc_resultado",
            aprovado=True,
            tipo_dip="4x4",
            inicio="nova",
        )
    )


def test_consumo_parcial_atualiza_a_cada_tel(client, db_session: Session):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa, inicio = _alinhar_inicio(db_session, criado["execucao_id"])
    _telemetria(client, _hc_aprovado(), inicio + timedelta(seconds=1))
    db_session.refresh(tentativa)

    tel1 = _evento(
        2,
        {
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 8400,
            "vel_mm_s": 200,
            "eixo_longo": None,
        },
    )
    _telemetria(client, tel1, inicio + timedelta(seconds=5))
    db_session.refresh(tentativa)
    assert tentativa.bateria_inicial == Decimal("8400")
    assert tentativa.consumo_bateria == Decimal("0")
    assert tentativa.consumo_bateria_pct == Decimal("0")

    tel2 = _evento(
        3,
        {
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 7800,
            "vel_mm_s": 200,
            "eixo_longo": None,
        },
    )
    _telemetria(client, tel2, inicio + timedelta(seconds=6))
    db_session.refresh(tentativa)
    assert tentativa.bateria_final == Decimal("7800")
    assert tentativa.consumo_bateria == Decimal("600")
    assert tentativa.consumo_bateria_pct == Decimal("45")


def test_velocidade_e_tempo_parcial_apos_passos(client, db_session: Session):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa, inicio = _alinhar_inicio(db_session, criado["execucao_id"])
    _telemetria(client, _hc_aprovado(), inicio + timedelta(seconds=1))
    db_session.refresh(tentativa)
    execucao = tentativa.execucao

    for seq, (x, y, instante) in enumerate([(0, 0, 10), (0, 1, 20)], start=2):
        linha = _evento(seq, {"tipo": "passo", "x": x, "y": y, "paredes": 5})
        _telemetria(client, linha, inicio + timedelta(seconds=instante))

    db_session.refresh(tentativa)
    db_session.refresh(execucao)
    assert tentativa.tempo_s == Decimal("20")
    assert tentativa.velocidade_media == Decimal("0.018")
    assert execucao.tempo_total_s == Decimal("20")


def test_metricas_finais_no_sucesso(client, db_session: Session):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa, inicio = _alinhar_inicio(db_session, criado["execucao_id"])
    _telemetria(client, _hc_aprovado(), inicio + timedelta(seconds=1))
    db_session.refresh(tentativa)
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(criado["execucao_id"]))

    _telemetria(
        client,
        _evento(
            2,
            {
                "tipo": "tel",
                "estado": "running",
                "x": 0,
                "y": 0,
                "rumo": "N",
                "bat_mv": 8400,
                "vel_mm_s": 200,
                "eixo_longo": None,
            },
        ),
        inicio + timedelta(seconds=5),
    )
    _telemetria(
        client,
        escrever_linha(Sucesso(v=1, boot=9, seq=3, t_ms=300, tipo="sucesso", x=3, y=3)),
        inicio + timedelta(seconds=30),
    )

    db_session.refresh(tentativa)
    db_session.refresh(execucao)
    assert tentativa.status == "success"
    assert tentativa.tempo_s == Decimal("30")
    assert tentativa.consumo_bateria == Decimal("0")
    assert execucao.status == "concluida"
    assert execucao.tempo_total_s == Decimal("30")
