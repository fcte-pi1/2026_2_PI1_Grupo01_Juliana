import json
import logging
import uuid
from datetime import UTC, datetime

import pytest
from app.models import ExecucaoLogica, Labirinto, Tentativa
from app.repositories import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from sqlalchemy import select

RECEBIDO_EM = "2026-10-08T12:00:00+00:00"
TEL = {
    "v": 1,
    "boot": 7,
    "seq": 42,
    "t_ms": 11250,
    "tipo": "tel",
    "estado": "running",
    "x": 0,
    "y": 2,
    "rumo": "N",
    "bat_mv": 7810,
    "vel_mm_s": 210,
    "eixo_longo": None,
}


def _linha(**campos) -> str:
    return json.dumps({**TEL, **campos}, separators=(",", ":"))


def _enviar(client, linha: str):
    return client.post("/telemetria", json={"linha": linha, "recebido_em": RECEBIDO_EM})


def _nova_execucao(db_session, tipo: str) -> ExecucaoLogica:
    labirinto_id = db_session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == tipo))
    execucao = ExecucaoLogica(
        execucao_id=uuid.uuid4(),
        labirinto_id=labirinto_id,
        status="em_andamento",
        tentativas_usadas=1,
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(execucao)
    return execucao


def _nova_tentativa(db_session, execucao: ExecucaoLogica, status: str) -> Tentativa:
    tentativa = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=1,
        status=status,
        tipo_inicio="nova",
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(tentativa)
    return tentativa


@pytest.fixture(params=["4x4", "8x4", "12x4"])
def tentativa_aberta(request, db_session) -> Tentativa:
    execucao = _nova_execucao(db_session, request.param)
    return _nova_tentativa(db_session, execucao, "running")


LINHAS_INVALIDAS = {
    "json_invalido": '{"v":1,"tipo":"tel"',
    "campo_faltando": json.dumps({k: v for k, v in TEL.items() if k != "bat_mv"}),
    "x_12": _linha(x=12),
    "versao_2": _linha(v=2),
    "300_bytes": _linha() + " " * (300 - len(_linha())),
}


@pytest.mark.parametrize("linha", LINHAS_INVALIDAS.values(), ids=LINHAS_INVALIDAS.keys())
def test_linha_invalida_da_422_e_a_proxima_valida_e_aceita(client, tentativa_aberta, caplog, linha):
    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, linha)

    assert resposta.status_code == 422
    assert resposta.json()["detail"]
    assert any(r.levelno == logging.WARNING for r in caplog.records)

    resposta = _enviar(client, _linha())
    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}


def test_linha_de_300_bytes_tem_mesmo_300_bytes():
    assert len(LINHAS_INVALIDAS["300_bytes"].encode()) == 300


def test_log_da_linha_invalida_trunca_em_256_caracteres(client, caplog):
    linha = "x" * 1000
    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, linha)

    assert resposta.status_code == 422
    registro = next(r for r in caplog.records if r.name == "app.telemetria")
    assert "x" * 256 in registro.getMessage()
    assert "x" * 257 not in registro.getMessage()


def test_sem_tentativa_aberta_descarta_com_200(client, db_session, caplog):
    execucao = _nova_execucao(db_session, "4x4")
    _nova_tentativa(db_session, execucao, "success")

    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        resposta = _enviar(client, _linha())

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    assert TelemetriaRepository(db_session).contar_leituras() == 0
    assert any(
        r.levelno == logging.INFO and "nenhuma tentativa aberta" in r.getMessage()
        for r in caplog.records
    )


def test_tentativa_aberta_e_a_mais_recente_com_labirinto(db_session):
    antiga = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    antiga.iniciada_em = datetime(2026, 10, 1, tzinfo=UTC)
    recente = _nova_tentativa(db_session, _nova_execucao(db_session, "8x4"), "health-check")
    _nova_tentativa(db_session, _nova_execucao(db_session, "12x4"), "failed")
    db_session.flush()

    aberta = Repositorio(db_session).buscar_tentativa_aberta()

    assert aberta is not None
    assert aberta.tentativa_id == recente.tentativa_id
    assert aberta.execucao.labirinto.tipo == "8x4"
