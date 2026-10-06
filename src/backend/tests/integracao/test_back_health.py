def test_back_health_com_banco(client):
    response = client.get("/back-health")
    assert response.status_code == 200
    body = response.json()
    assert body["banco_acessivel"] is True
    assert body["status"] == "ok"
