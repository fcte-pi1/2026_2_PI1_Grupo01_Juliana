"""Metricas parciais na ingestao e finais no encerramento (BACK-04)."""

from __future__ import annotations

import json
import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

from app.models import ExecucaoLogica, Tentativa
from contrato.telemetria import Sucesso, escrever_linha
from sqlalchemy import select
from sqlalchemy.orm import Session
from tests.integracao.apoio_health_check import gravar_health_check


def _t0() -> datetime:
    """Início recente para o monitor RF32 não encerrar um teste de métricas."""
    return datetime.now(UTC)


def _iso(dt: datetime) -> str:
    return dt.isoformat().replace("+00:00", "Z")


def _telemetria(
    client,
    linha: str,
    agora_backend: datetime,
    *,
    recebido_em: datetime | None = None,
):
    client.app.state.ingestao_telemetria.relogio_backend = lambda: agora_backend
    return client.post(
        "/telemetria",
        json={"linha": linha.strip(), "recebido_em": _iso(recebido_em or agora_backend)},
    )


def _evento(seq: int, campos: dict) -> str:
    return json.dumps({"v": 1, "boot": 9, "seq": seq, "t_ms": 1000 + seq * 100} | campos)


def _alinhar_inicio(client, db_session: Session, execucao_id: str) -> tuple[Tentativa, datetime]:
    inicio = _t0()
    # sinal recente para o monitor de link não cancelar a tentativa aberta há 60 s (RF32)
    client.app.state.monitor_conexao.registrar_mensagem()
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
    tentativa = db_session.scalar(
        select(Tentativa).where(Tentativa.execucao_id == execucao.execucao_id)
    )
    execucao.iniciada_em = inicio
    tentativa.iniciada_em = inicio
    db_session.commit()
    return tentativa, inicio


def _nova_execucao(client, db_session: Session) -> dict:
    gravar_health_check(db_session, "4x4")
    return client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()


def _tel(seq: int, estado: str = "running") -> str:
    return _evento(
        seq,
        {
            "tipo": "tel",
            "estado": estado,
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 8400,
            "vel_mm_s": 0,
            "eixo_longo": None,
        },
    )


def test_tempo_conta_da_largada_e_nao_do_health_check(client, db_session: Session):
    # health-check 5 min antes da largada, dentro dos 7 min de validade
    gravar_health_check(db_session, "4x4", concluido_em=datetime.now(UTC) - timedelta(minutes=5))
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])

    _telemetria(
        client,
        _evento(1, {"tipo": "passo", "x": 0, "y": 0, "paredes": 5}),
        inicio + timedelta(seconds=12),
    )

    db_session.refresh(tentativa)
    assert tentativa.status == "running"
    assert tentativa.tempo_s == Decimal("12")


def test_largar_vai_nas_respostas_ate_o_robo_correr(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    _, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])

    parado = _telemetria(client, _tel(1, estado="health-check"), inicio + timedelta(seconds=1))
    correndo = _telemetria(client, _tel(2), inicio + timedelta(seconds=2))
    depois = _telemetria(client, _tel(3), inicio + timedelta(seconds=3))

    assert parado.json() == {"comandos": [{"v": 1, "cmd": "largar"}]}
    assert correndo.json() == {"comandos": []}
    assert depois.json() == {"comandos": []}


def test_consumo_parcial_atualiza_a_cada_tel(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])
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


def test_consumo_percentual_usa_curva_aa(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])

    for seq, bateria, instante in [(2, 6400, 5), (3, 5600, 6)]:
        _telemetria(
            client,
            _evento(
                seq,
                {
                    "tipo": "tel",
                    "estado": "running",
                    "x": 0,
                    "y": 0,
                    "rumo": "N",
                    "bat_mv": bateria,
                    "vel_mm_s": 200,
                    "eixo_longo": None,
                },
            ),
            inicio + timedelta(seconds=instante),
        )

    db_session.refresh(tentativa)
    assert tentativa.bateria_inicial == Decimal("6400")
    assert tentativa.bateria_final == Decimal("5600")
    assert tentativa.consumo_bateria_pct == Decimal("50")


def test_velocidade_conta_revisitas_e_desconsidera_largada(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])
    db_session.refresh(tentativa)
    execucao = tentativa.execucao

    percurso = [(0, 0, 5), (0, 1, 10), (0, 0, 15), (0, 1, 20)]
    for seq, (x, y, instante) in enumerate(percurso, start=2):
        linha = _evento(seq, {"tipo": "passo", "x": x, "y": y, "paredes": 5})
        _telemetria(client, linha, inicio + timedelta(seconds=instante))

    db_session.refresh(tentativa)
    db_session.refresh(execucao)
    assert tentativa.tempo_s == Decimal("20")
    assert tentativa.velocidade_media == Decimal("0.027")
    assert execucao.tempo_total_s == Decimal("20")


def test_tempo_usa_relogio_backend_mesmo_com_ponte_defasada(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])
    relogio_ponte = inicio - timedelta(hours=2)
    _telemetria(
        client,
        _evento(2, {"tipo": "passo", "x": 0, "y": 0, "paredes": 5}),
        inicio + timedelta(seconds=20),
        recebido_em=relogio_ponte + timedelta(seconds=1),
    )

    db_session.refresh(tentativa)
    assert tentativa.tempo_s == Decimal("20")


def test_metricas_finais_no_sucesso(client, db_session: Session):
    criado = _nova_execucao(client, db_session)
    tentativa, inicio = _alinhar_inicio(client, db_session, criado["execucao_id"])
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
