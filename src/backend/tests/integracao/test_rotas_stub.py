EXECUCAO_ID = "7c9e6679-7425-40de-944b-e07fc1f90ae7"


def test_post_execucao_retorna_501(client):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 501


def test_get_execucoes_retorna_501(client):
    response = client.get("/execucoes")
    assert response.status_code == 501


def test_get_execucoes_com_filtro_retorna_501(client):
    response = client.get("/execucoes", params={"tipo_labirinto": "4x4"})
    assert response.status_code == 501


def test_get_em_andamento_retorna_501(client):
    response = client.get("/execucoes/em-andamento")
    assert response.status_code == 501


def test_get_execucao_por_id_retorna_501(client):
    response = client.get(f"/execucoes/{EXECUCAO_ID}")
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
            "linha": '{"v":1,"boot":1,"seq":1,"t_ms":0,"tipo":"tel","estado":"running","x":0,"y":0,"rumo":"N","bat_mv":7800,"vel_mm_s":0,"eixo_longo":null}',
            "recebido_em": "2026-10-07T14:03:21.512-03:00",
        },
    )
    assert response.status_code == 501
