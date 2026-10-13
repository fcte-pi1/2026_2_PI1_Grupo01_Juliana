"""Respostas 422 de POST /telemetria validadas contra o openapi.yaml."""

import pytest
from tests.contrato_openapi import validador, validar_resposta

RECEBIDO_EM = "2026-10-13T17:00:00.000-03:00"


def _casa_com(corpo, esquema: str) -> bool:
    return validador({"$ref": f"#/components/schemas/{esquema}"}).is_valid(corpo)


def test_linha_invalida_segue_o_contrato(client):
    response = client.post("/telemetria", json={"linha": "{quebrado", "recebido_em": RECEBIDO_EM})
    assert response.status_code == 422
    validar_resposta(response, "/telemetria", "post")
    corpo = response.json()
    assert isinstance(corpo["detail"], str)
    assert isinstance(corpo["comandos"], list)
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
    assert not _casa_com(corpo, "ErroLinhaInvalida")
