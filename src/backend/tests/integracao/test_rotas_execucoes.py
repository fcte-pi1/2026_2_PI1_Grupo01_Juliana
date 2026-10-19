from datetime import UTC, datetime, timedelta
from uuid import uuid4

import pytest
from app.main import app
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from tests.contrato_openapi import validar_resposta
from tests.integracao.apoio_health_check import gravar_health_check


@pytest.fixture
def gerenciador() -> GerenciadorExecucoes:
    return app.state.gerenciador


@pytest.fixture
def health_check(db_session):
    return gravar_health_check(db_session, "4x4")


def test_get_em_andamento_vazio(client):
    assert client.get("/execucoes/em-andamento").status_code == 204


def test_post_execucao_cria(client, health_check):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 201
    body = response.json()
    assert body["attempt_index"] == 1
    assert "execucao_id" in body
    assert "tentativa_id" in body


def test_post_execucao_sem_health_check_409(client):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 409
    assert response.json()["motivo"] == "health_check_ausente"
    validar_resposta(response, "/execucoes", "post")
    assert client.get("/execucoes/em-andamento").status_code == 204


def test_get_em_andamento_com_execucao(client, health_check):
    client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    r = client.get("/execucoes/em-andamento")
    assert r.status_code == 200
    assert r.json()["status"] == "em_andamento"
    [tentativa] = r.json()["tentativas"]
    assert tentativa["status"] == "running"


def test_encerrar_tentativa_sem_aberta_409(client, health_check):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    execucao_id = criado["execucao_id"]
    assert (
        client.post(
            f"/execucoes/{execucao_id}/encerrar-tentativa", json={"motivo": "collision"}
        ).status_code
        == 200
    )
    response = client.post(
        f"/execucoes/{execucao_id}/encerrar-tentativa",
        json={"motivo": "collision"},
    )
    assert response.status_code == 409
    assert response.json() == {"detail": "Não há tentativa aberta para encerrar."}


def test_encerrar_execucao_cancela_tentativa_aberta(client, db_session, gerenciador, health_check):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    resposta = client.post(f"/execucoes/{criado['execucao_id']}/encerrar")
    assert resposta.status_code == 200
    validar_resposta(resposta, "/execucoes/{execucao_id}/encerrar", "post")
    corpo = resposta.json()
    assert corpo["status"] == "cancelada"
    assert corpo["tentativas"][0]["status"] == "failed"
    assert corpo["tentativas"][0]["falha"]["motivo"] == "encerrado_operador"
    assert corpo["tentativas"][0]["falha"]["origem"] == "encerrado_operador"
    db_session.expire_all()
    assert [c.model_dump() for c in gerenciador.comandos_pendentes()] == [
        {"v": 1, "cmd": "interromper"}
    ]
    assert client.get("/execucoes/em-andamento").status_code == 204
    recusa = client.post(f"/execucoes/{criado['execucao_id']}/retomar")
    assert recusa.status_code == 409
    assert recusa.json()["motivo"] == "execucao_cancelada"
    de_novo = client.post(f"/execucoes/{criado['execucao_id']}/encerrar")
    assert de_novo.status_code == 409
    assert de_novo.json()["detail"] == "A execução não está em andamento."


def test_encerrar_execucao_apos_tentativa_fechada(client, health_check):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    fechada = client.post(
        f"/execucoes/{criado['execucao_id']}/encerrar-tentativa",
        json={"motivo": "stuck"},
    )
    assert fechada.status_code == 200
    resposta = client.post(f"/execucoes/{criado['execucao_id']}/encerrar")
    assert resposta.status_code == 200
    corpo = resposta.json()
    assert corpo["status"] == "cancelada"
    assert corpo["tentativas"][0]["falha"]["motivo"] == "stuck"
    recusa = client.post(f"/execucoes/{criado['execucao_id']}/retomar")
    assert recusa.status_code == 409
    assert recusa.json()["motivo"] == "execucao_cancelada"


def test_encerrar_execucao_inexistente_404(client):
    response = client.post(f"/execucoes/{uuid4()}/encerrar")
    assert response.status_code == 404
    assert response.json()["detail"] == "Execução não encontrada."


def test_encerrar_corpo_invalido_continua_422(client, health_check):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    response = client.post(
        f"/execucoes/{criado['execucao_id']}/encerrar-tentativa",
        json={"motivo": "link_lost"},
    )
    assert response.status_code == 422
    assert isinstance(response.json()["detail"], list)


def test_retomar_execucao_inexistente_404(client):
    response = client.post(f"/execucoes/{uuid4()}/retomar")
    assert response.status_code == 404


def test_ultimo_health_check_vazio(client):
    assert client.get("/health-check/ultimo").status_code == 204


def test_ultimo_health_check_aprovado_tem_validade(client, db_session):
    concluido_em = datetime.now(UTC).replace(microsecond=0)
    gravar_health_check(db_session, "8x4", concluido_em=concluido_em)

    resposta = client.get("/health-check/ultimo")
    validar_resposta(resposta, "/health-check/ultimo")
    corpo = resposta.json()

    assert corpo["aprovado"] is True
    assert corpo["tipo_dip"] == "8x4"
    valido_ate = datetime.fromisoformat(corpo["valido_ate"])
    assert valido_ate == concluido_em + timedelta(seconds=420)


def test_ultimo_health_check_reprovado_sem_validade(client, db_session):
    gravar_health_check(db_session, "4x4", aprovado=False)

    corpo = client.get("/health-check/ultimo").json()

    assert corpo["aprovado"] is False
    assert corpo["valido_ate"] is None
