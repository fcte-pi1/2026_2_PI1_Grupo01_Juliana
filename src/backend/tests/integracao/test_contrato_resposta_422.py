"""Respostas 422 de POST /telemetria validadas contra o openapi.yaml."""

import pytest
from app.main import app
from app.routers.telemetria import DESCRICAO, DESCRICAO_422, EXEMPLOS_422, RESUMO
from contrato.telemetria import Comando
from tests.contrato_openapi import OPENAPI, validador, validar_resposta

RECEBIDO_EM = "2026-10-13T17:00:00.000-03:00"
EXEMPLOS_LINHA = {"linha-invalida", "linha-invalida-interrupcao-pendente"}
EXEMPLOS_CORPO = {"corpo-nao-json", "corpo-sem-linha"}


def _casa_com(corpo, esquema: str) -> bool:
    return validador({"$ref": f"#/components/schemas/{esquema}"}).is_valid(corpo)


def _operacao_contrato() -> dict:
    return OPENAPI["paths"]["/telemetria"]["post"]


def _resposta_422(operacao: dict) -> dict:
    return operacao["responses"]["422"]["content"]["application/json"]


def test_linha_invalida_segue_o_contrato(client):
    response = client.post("/telemetria", json={"linha": "{quebrado", "recebido_em": RECEBIDO_EM})
    assert response.status_code == 422
    validar_resposta(response, "/telemetria", "post")
    corpo = response.json()
    assert isinstance(corpo["detail"], str)
    assert corpo["comandos"] == []
    assert _casa_com(corpo, "ErroLinhaInvalida")
    assert not _casa_com(corpo, "ErroValidacao")


def test_linha_invalida_com_interrupcao_segue_o_contrato(client, monkeypatch):
    monkeypatch.setattr(client.app.state.gerenciador, "comandos_pendentes", lambda: [Comando()])

    response = client.post("/telemetria", json={"linha": "{quebrado", "recebido_em": RECEBIDO_EM})

    assert response.status_code == 422
    validar_resposta(response, "/telemetria", "post")
    corpo = response.json()
    assert isinstance(corpo["detail"], str)
    assert corpo["comandos"] == [{"v": 1, "cmd": "interromper"}]
    assert _casa_com(corpo, "ErroLinhaInvalida")
    assert not _casa_com(corpo, "ErroValidacao")


@pytest.mark.parametrize(
    "envio",
    [
        {"json": {"recebido_em": RECEBIDO_EM}},
        {"json": {"linha": 42, "recebido_em": RECEBIDO_EM}},
        {"content": b"{quebrado", "headers": {"content-type": "application/json"}},
    ],
    ids=["sem-linha", "linha-nao-texto", "corpo-nao-json"],
)
def test_corpo_malformado_segue_o_contrato(client, envio):
    response = client.post("/telemetria", **envio)
    assert response.status_code == 422
    validar_resposta(response, "/telemetria", "post")
    corpo = response.json()
    assert isinstance(corpo["detail"], list)
    assert _casa_com(corpo, "ErroValidacao")
    assert not _casa_com(corpo, "ErroLinhaInvalida")


def test_exemplos_do_422_cabem_no_esquema_certo():
    exemplos = _resposta_422(_operacao_contrato())["examples"]
    esquema = _resposta_422(_operacao_contrato())["schema"]
    assert set(exemplos) == EXEMPLOS_LINHA | EXEMPLOS_CORPO
    for nome, exemplo in exemplos.items():
        validador(esquema).validate(exemplo["value"])
        if nome in EXEMPLOS_LINHA:
            assert _casa_com(exemplo["value"], "ErroLinhaInvalida")
            assert not _casa_com(exemplo["value"], "ErroValidacao")
        else:
            assert _casa_com(exemplo["value"], "ErroValidacao")
            assert not _casa_com(exemplo["value"], "ErroLinhaInvalida")


def test_docs_publica_o_mesmo_422_do_contrato():
    contrato = _operacao_contrato()
    publicado = app.openapi()["paths"]["/telemetria"]["post"]
    assert RESUMO == contrato["summary"]
    assert DESCRICAO == contrato["description"]
    assert DESCRICAO_422 == contrato["responses"]["422"]["description"]
    assert EXEMPLOS_422 == _resposta_422(contrato)["examples"]
    assert publicado["summary"] == contrato["summary"]
    assert publicado["description"] == contrato["description"]
    assert publicado["responses"]["422"]["description"] == DESCRICAO_422
    assert _resposta_422(publicado)["examples"] == EXEMPLOS_422
    refs = [item["$ref"].rsplit("/", 1)[-1] for item in _resposta_422(publicado)["schema"]["oneOf"]]
    assert refs == ["ErroLinhaInvalida", "ErroValidacao"]
    for nome in refs:
        assert nome in app.openapi()["components"]["schemas"]
