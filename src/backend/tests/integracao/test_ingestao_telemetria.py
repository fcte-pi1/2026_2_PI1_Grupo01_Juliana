import json
import logging
import uuid
from datetime import UTC, datetime, timedelta

import pytest
from app.models import ExecucaoLogica, Labirinto, LeituraTelemetria, Tentativa
from app.repositories import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from contrato.telemetria import ler_linha
from sqlalchemy import select
from sqlalchemy.exc import OperationalError

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


def _leituras(db_session, tentativa: Tentativa) -> list[LeituraTelemetria]:
    stmt = (
        select(LeituraTelemetria)
        .where(LeituraTelemetria.tentativa_id == tentativa.tentativa_id)
        .order_by(LeituraTelemetria.ordem)
    )
    return list(db_session.scalars(stmt))


def _passo(seq: int, **campos) -> str:
    return json.dumps(
        {"v": 1, "boot": 7, "seq": seq, "t_ms": seq * 10, "tipo": "passo", "x": 0, "y": 1}
        | {"paredes": 5}
        | campos,
        separators=(",", ":"),
    )


@pytest.mark.parametrize(
    ("tipo", "x", "y", "status_esperado"),
    [
        ("4x4", 4, 0, 422),
        ("4x4", 3, 3, 200),
        ("8x4", 7, 2, 200),
        ("8x4", 2, 7, 200),
        ("8x4", 5, 5, 422),
        ("12x4", 11, 3, 200),
        ("12x4", 4, 4, 422),
    ],
)
def test_limites_do_labirinto(client, db_session, caplog, tipo, x, y, status_esperado):
    _nova_tentativa(db_session, _nova_execucao(db_session, tipo), "running")

    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, _linha(x=x, y=y))

    assert resposta.status_code == status_esperado
    if status_esperado == 422:
        assert "fora do labirinto" in resposta.json()["detail"]
        assert TelemetriaRepository(db_session).contar_leituras() == 0
        assert any(r.levelno == logging.WARNING for r in caplog.records)


@pytest.mark.parametrize("campos", [{"tipo": "passo", "paredes": 0}, {"tipo": "sucesso"}])
def test_limites_valem_para_passo_e_sucesso(client, db_session, campos):
    _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    linha = json.dumps({"v": 1, "boot": 7, "seq": 1, "t_ms": 1, "x": 0, "y": 4} | campos)

    assert _enviar(client, linha).status_code == 422


def test_limites_valem_para_falha(client, db_session):
    _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    falha = {"tipo": "falha", "motivo": "stuck", "origem": "automatica", "componente": None}
    linha = json.dumps({"v": 1, "boot": 7, "seq": 1, "t_ms": 1, "x": 5, "y": 0} | falha)

    assert _enviar(client, linha).status_code == 422


def test_tel_grava_leitura_telemetria(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "health-check")

    _enviar(client, _linha(t_ms=1000))
    resposta = client.post(
        "/telemetria",
        json={"linha": _linha(t_ms=1500, x=1, estado="running"), "recebido_em": RECEBIDO_EM},
    )

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    primeira, segunda = _leituras(db_session, tentativa)
    assert (primeira.ordem, segunda.ordem) == (1, 2)
    # a fase é o status da tentativa, não o estado da tel
    assert segunda.fase == "health-check"
    assert (segunda.x, segunda.y) == (1, 2)
    assert (segunda.bateria, segunda.velocidade) == (7810, 210)
    recebido_em = datetime.fromisoformat(RECEBIDO_EM)
    assert segunda.recebido_em == recebido_em
    # âncora fixada na primeira mensagem do boot: 12:00:00 - 1 s
    assert primeira.enviado_em == recebido_em
    assert segunda.enviado_em == recebido_em + timedelta(milliseconds=500)


def test_ordem_e_por_tentativa(client, db_session):
    antiga = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    _enviar(client, _linha())
    antiga.status = "failed"
    nova = _nova_tentativa(db_session, _nova_execucao(db_session, "8x4"), "running")
    db_session.flush()

    _enviar(client, _linha())

    assert [leitura.ordem for leitura in _leituras(db_session, nova)] == [1]
    assert [leitura.ordem for leitura in _leituras(db_session, antiga)] == [1]


def test_tel_com_mesmo_seq_nunca_e_repetida(client, tentativa_aberta, db_session):
    _enviar(client, _linha())
    _enviar(client, _linha())

    assert len(_leituras(db_session, tentativa_aberta)) == 2


def test_evento_repetido_devolve_200_e_loga_info(client, tentativa_aberta, caplog):
    assert _enviar(client, _passo(1)).status_code == 200

    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        resposta = _enviar(client, _passo(1))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    assert any(r.levelno == logging.INFO and "repetida" in r.getMessage() for r in caplog.records)


def test_seq_so_e_confirmado_depois_do_commit(client, db_session, monkeypatch, caplog):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    # fecha o savepoint: o rollback da ingestão não pode desfazer a tentativa
    db_session.commit()
    commit_original = db_session.commit

    def commit_falho():
        raise OperationalError("COMMIT", {}, Exception("banco caiu"))

    monkeypatch.setattr(db_session, "commit", commit_falho)
    with pytest.raises(OperationalError):
        _enviar(client, _passo(1))
    with pytest.raises(OperationalError):
        _enviar(client, _linha())
    assert _leituras(db_session, tentativa) == []

    monkeypatch.setattr(db_session, "commit", commit_original)
    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        assert _enviar(client, _passo(1)).status_code == 200
        assert _enviar(client, _linha()).status_code == 200

    assert not any("repetida" in r.getMessage() for r in caplog.records)
    assert [leitura.ordem for leitura in _leituras(db_session, tentativa)] == [1]
    assert client.app.state.deduplicador.eh_repetida(ler_linha(_passo(1)))


def test_mensagem_valida_atualiza_monitor_de_conexao(client):
    monitor = client.app.state.monitor_conexao
    assert monitor.segundos_sem_sinal() is None

    _enviar(client, _linha())

    agora = datetime.fromisoformat(RECEBIDO_EM) + timedelta(seconds=3)
    assert monitor.segundos_sem_sinal(agora) == 3
