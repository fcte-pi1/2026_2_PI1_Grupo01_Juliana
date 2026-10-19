from uuid import UUID, uuid4

import pytest
from app.main import app
from app.models import Tentativa
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from tests.contrato_openapi import validar_resposta


@pytest.fixture
def gerenciador() -> GerenciadorExecucoes:
    return app.state.gerenciador


def test_get_em_andamento_vazio(client):
    assert client.get("/execucoes/em-andamento").status_code == 204


def test_post_execucao_cria(client):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 201
    body = response.json()
    assert body["attempt_index"] == 1
    assert "execucao_id" in body
    assert "tentativa_id" in body


def test_get_em_andamento_com_execucao(client):
    client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    r = client.get("/execucoes/em-andamento")
    assert r.status_code == 200
    assert r.json()["status"] == "em_andamento"


def test_encerrar_tentativa_sem_aberta_409(client, db_session, gerenciador):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    execucao_id = criado["execucao_id"]
    tentativa = db_session.get(Tentativa, UUID(criado["tentativa_id"]))
    gerenciador.aplicar_evento(db_session, tentativa, _hc_reprovado())
    db_session.commit()
    response = client.post(
        f"/execucoes/{execucao_id}/encerrar-tentativa",
        json={"motivo": "collision"},
    )
    assert response.status_code == 409
    assert response.json() == {"detail": "Não há tentativa aberta para encerrar."}


def test_encerrar_execucao_cancela_tentativa_aberta(client, db_session, gerenciador):
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


def test_encerrar_execucao_apos_tentativa_fechada(client):
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


def test_encerrar_corpo_invalido_continua_422(client):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    response = client.post(
        f"/execucoes/{criado['execucao_id']}/encerrar-tentativa",
        json={"motivo": "link_lost"},
    )
    assert response.status_code == 422
    assert isinstance(response.json()["detail"], list)


def _hc_reprovado():
    from contrato.telemetria import HcResultado

    return HcResultado(
        v=1,
        boot=1,
        seq=1,
        t_ms=1,
        tipo="hc_resultado",
        aprovado=False,
        tipo_dip="4x4",
        inicio="nova",
    )


def test_retomar_execucao_inexistente_404(client):
    response = client.post(f"/execucoes/{uuid4()}/retomar")
    assert response.status_code == 404
