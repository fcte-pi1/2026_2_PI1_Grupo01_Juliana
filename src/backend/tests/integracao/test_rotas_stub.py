EXECUCAO_ID = "7c9e6679-7425-40de-944b-e07fc1f90ae7"


def test_post_execucao_retorna_501(client):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 501


def test_get_em_andamento_retorna_501(client):
    response = client.get("/execucoes/em-andamento")
    assert response.status_code == 501


def test_encerrar_tentativa_retorna_501(client):
    response = client.post(
        f"/execucoes/{EXECUCAO_ID}/encerrar-tentativa",
        json={"motivo": "collision"},
    )
    assert response.status_code == 501


def test_retomar_execucao_retorna_501(client):
    response = client.post(f"/execucoes/{EXECUCAO_ID}/retomar")
    assert response.status_code == 501


def test_stream_execucao_retorna_501(client):
    response = client.get(f"/execucoes/{EXECUCAO_ID}/stream")
    assert response.status_code == 501


def test_post_telemetria_retorna_501(client):
    response = client.post(
        "/telemetria",
        json={
            "seq": 1,
            "status": "running",
            "x": 0,
            "y": 0,
            "bateria": 7.8,
            "velocidade": 0.0,
            "enviado_em": "2026-10-07T14:03:21.512-03:00",
        },
    )
    assert response.status_code == 501
