from app.services.estado_ponte import VALIDADE_DO_RELATO_S, EstadoPonte


def test_sem_relato_a_ponte_conta_como_desligada(client):
    corpo = client.get("/robo/estado").json()
    assert corpo["ponte"] == "desligada"
    assert corpo["porta"] is None
    assert corpo["reconexao_pedida"] is False


def test_o_relato_da_ponte_aparece_no_estado(client):
    resposta = client.post("/robo/ponte", json={"estado": "conectada", "porta": "/dev/rfcomm0"})
    assert resposta.status_code == 200
    assert resposta.json() == {"reconectar": False}
    corpo = client.get("/robo/estado").json()
    assert corpo["ponte"] == "conectada"
    assert corpo["porta"] == "/dev/rfcomm0"


def test_relato_invalido_e_recusado(client):
    assert client.post("/robo/ponte", json={"estado": "voando", "porta": "x"}).status_code == 422
    assert client.post("/robo/ponte", json={"estado": "conectada", "porta": ""}).status_code == 422


def test_reconectar_sem_ponte_ligada_responde_409(client):
    resposta = client.post("/robo/reconectar")
    assert resposta.status_code == 409
    assert resposta.json()["detail"] == "A ponte não está ligada."


def test_o_pedido_de_reconexao_chega_a_ponte_uma_unica_vez(client):
    client.post("/robo/ponte", json={"estado": "conectada", "porta": "/dev/rfcomm0"})
    pedido = client.post("/robo/reconectar")
    assert pedido.status_code == 202
    assert pedido.json() == {"aceito": True}
    assert client.get("/robo/estado").json()["reconexao_pedida"] is True

    primeira = client.post("/robo/ponte", json={"estado": "conectada", "porta": "/dev/rfcomm0"})
    segunda = client.post("/robo/ponte", json={"estado": "reconectando", "porta": "/dev/rfcomm0"})
    assert primeira.json() == {"reconectar": True}
    assert segunda.json() == {"reconectar": False}
    corpo = client.get("/robo/estado").json()
    assert corpo["reconexao_pedida"] is False
    assert corpo["ponte"] == "reconectando"


def test_relato_antigo_volta_a_desligada():
    agora = [0.0]
    from datetime import UTC, datetime, timedelta

    base = datetime(2026, 11, 21, 12, 0, tzinfo=UTC)
    estado = EstadoPonte(relogio=lambda: base + timedelta(seconds=agora[0]))
    estado.registrar("conectada", "/dev/rfcomm0")
    assert estado.situacao() == ("conectada", "/dev/rfcomm0")
    agora[0] = VALIDADE_DO_RELATO_S + 1
    assert estado.situacao() == ("desligada", "/dev/rfcomm0")
    assert estado.pedir_reconexao() is False
