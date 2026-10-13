from uuid import UUID, uuid4

import pytest
from app.main import app
from app.models import Tentativa
from app.services.gerenciador_execucoes import GerenciadorExecucoes


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


def test_encerrar_tentativa_sem_aberta_422(client, db_session, gerenciador):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    execucao_id = criado["execucao_id"]
    tentativa = db_session.get(Tentativa, UUID(criado["tentativa_id"]))
    gerenciador.aplicar_evento(db_session, tentativa, _hc_reprovado())
    db_session.commit()
    response = client.post(
        f"/execucoes/{execucao_id}/encerrar-tentativa",
        json={"motivo": "collision"},
    )
    assert response.status_code == 422


def _hc_reprovado():
    from contrato.telemetria import HcResultado

    return HcResultado(
        v=1, boot=1, seq=1, t_ms=1, tipo="hc_resultado", aprovado=False, tipo_dip="4x4", inicio="nova"
    )


def test_retomar_execucao_inexistente_404(client):
    response = client.post(f"/execucoes/{uuid4()}/retomar")
    assert response.status_code == 404
