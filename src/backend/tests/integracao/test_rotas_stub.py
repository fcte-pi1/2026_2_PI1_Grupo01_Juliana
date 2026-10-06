def test_post_execucao_retorna_501(client):
    response = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert response.status_code == 501
