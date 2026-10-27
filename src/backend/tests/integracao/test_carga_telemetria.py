"""RNF-B11: a ingestão aguenta a rajada de telemetria sem atraso acumulado."""

import time
import uuid
from datetime import UTC, datetime

import pytest
from app.models import ExecucaoLogica, Labirinto, LeituraTelemetria, PassoTrajeto, Tentativa
from app.repositories import Repositorio
from scripts.carga_telemetria import (
    P95_MAXIMO_S,
    TentativaAbertaError,
    _p95,
    executar_carga,
    mensagem,
    sem_crescimento,
)
from sqlalchemy import func, select
from sqlalchemy.orm import sessionmaker

TOTAL = 600
JANELA = 100
RECEBIDO_EM = "2026-10-08T12:00:00+00:00"


def _abrir_tentativa(db_session) -> Tentativa:
    labirinto_id = db_session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == "4x4"))
    execucao = ExecucaoLogica(
        execucao_id=uuid.uuid4(),
        labirinto_id=labirinto_id,
        status="em_andamento",
        tentativas_usadas=1,
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(execucao)
    tentativa = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=1,
        status="running",
        tipo_inicio="nova",
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(tentativa)
    return tentativa


def _fabrica(db_session):
    """Sessões na mesma conexão do fixture, para enxergar o que o client grava."""
    return sessionmaker(bind=db_session.get_bind(), join_transaction_mode="create_savepoint")


def test_600_mensagens_sem_espera_ficam_abaixo_de_100_ms_sem_crescer(client, db_session):
    _abrir_tentativa(db_session)

    tempos = []
    for seq in range(TOTAL):
        corpo = {"linha": mensagem(50, seq, 1000 + 100 * seq), "recebido_em": RECEBIDO_EM}
        inicio = time.perf_counter()
        resposta = client.post("/telemetria", json=corpo)
        tempos.append(time.perf_counter() - inicio)
        assert resposta.status_code == 200, resposta.text

    p95 = _p95(tempos)
    p95_inicio = _p95(tempos[:JANELA])
    p95_fim = _p95(tempos[-JANELA:])
    assert p95 < P95_MAXIMO_S, f"p95 = {p95 * 1000:.1f} ms"
    assert sem_crescimento(tempos, JANELA), (
        f"p95 cresceu de {p95_inicio * 1000:.1f} ms para {p95_fim * 1000:.1f} ms"
    )


def test_executar_carga_aprova_e_fecha_a_propria_tentativa(client, db_session):
    resultado = executar_carga(client, _fabrica(db_session), taxa=50, duracao=1)

    assert resultado.aprovado, resultado
    assert len(resultado.registros) == 50
    db_session.expire_all()
    tentativa = db_session.scalar(
        select(Tentativa).where(Tentativa.execucao_id == resultado.execucao_id)
    )
    execucao = db_session.get(ExecucaoLogica, resultado.execucao_id)
    assert tentativa.status == "failed"
    assert execucao.status == "cancelada"
    leituras = db_session.scalar(
        select(func.count()).where(LeituraTelemetria.tentativa_id == tentativa.tentativa_id)
    )
    passos = db_session.scalar(
        select(func.count()).where(PassoTrajeto.tentativa_id == tentativa.tentativa_id)
    )
    assert leituras > 0
    assert passos > 0


def test_executar_carga_recusa_com_tentativa_ja_aberta(client, db_session):
    existente = _abrir_tentativa(db_session)

    with pytest.raises(TentativaAbertaError):
        executar_carga(client, _fabrica(db_session), taxa=50, duracao=1)

    db_session.expire_all()
    assert db_session.get(Tentativa, existente.tentativa_id).status == "running"
