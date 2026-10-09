"""Respostas de GET /execucoes e GET /execucoes/{id} validadas contra o openapi.yaml."""

import time
import uuid
from datetime import UTC, datetime, timedelta
from pathlib import Path

import pytest
import yaml
from openapi_schema_validator import OAS30Validator


def _nulo_explicito(no):
    """Lê `allOf: [$ref] + nullable: true` como "o $ref ou null".

    É a intenção do contrato, mas a leitura estrita da OAS 3.0.3 aplica o
    nullable só ao type do próprio nó, e o validador recusaria o null.
    """
    if isinstance(no, list):
        return [_nulo_explicito(item) for item in no]
    if not isinstance(no, dict):
        return no
    no = {chave: _nulo_explicito(valor) for chave, valor in no.items()}
    if no.get("nullable") and "allOf" in no and "type" not in no:
        resto = {k: v for k, v in no.items() if k not in ("allOf", "nullable")}
        return {**resto, "anyOf": [{"allOf": no["allOf"]}, {"enum": [None]}]}
    return no


OPENAPI = _nulo_explicito(
    yaml.safe_load(
        (Path(__file__).resolve().parents[2] / "openapi.yaml").read_text(encoding="utf-8")
    )
)
LIMITE_DETALHE_S = 0.3


def _validar(response, caminho: str, metodo: str = "get") -> None:
    resposta = OPENAPI["paths"][caminho][metodo]["responses"][str(response.status_code)]
    if "$ref" in resposta:
        resposta = OPENAPI["components"]["responses"][resposta["$ref"].rsplit("/", 1)[-1]]
    schema = resposta["content"]["application/json"]["schema"]
    validador = OAS30Validator(
        {**schema, "components": OPENAPI["components"]},
        format_checker=OAS30Validator.FORMAT_CHECKER,
    )
    validador.validate(response.json())
    for cabecalho in resposta.get("headers", {}):
        assert cabecalho in response.headers


def _popular(fabrica) -> list:
    inicio = datetime(2026, 10, 7, 17, 0, tzinfo=UTC)
    concluida = fabrica.execucao_12x4_com_3_tentativas(
        passos_por_tentativa=8, leituras_por_tentativa=5
    )
    andamento = fabrica.execucao(
        tipo="8x4", status="em_andamento", tentativas_usadas=1, iniciada_em=inicio
    )
    aberta = fabrica.tentativa(andamento, status="running", bateria_final=None)
    fabrica.health_check(aberta)
    fabrica.passos(aberta, [(0, 0), (0, 1)], paredes_mask=None)
    fabrica.leituras(aberta, 3)
    cancelada = fabrica.execucao(
        status="cancelada", tentativas_usadas=1, iniciada_em=inicio - timedelta(days=1)
    )
    reprovada = fabrica.tentativa(cancelada, status="failed")
    fabrica.health_check(reprovada, reprovado="tof_direito")
    fabrica.falha(reprovada, motivo="health_check_failed", celula=None)
    vazia = fabrica.execucao(status="cancelada", iniciada_em=inicio - timedelta(days=2))
    return [concluida, andamento, cancelada, vazia]


@pytest.mark.parametrize("params", [{}, {"tipo_labirinto": "12x4"}, {"limite": 1, "offset": 1}])
def test_lista_segue_o_contrato(client, fabrica, params):
    _popular(fabrica)
    response = client.get("/execucoes", params=params)
    assert response.status_code == 200
    _validar(response, "/execucoes")


def test_lista_com_parametro_invalido_segue_o_contrato(client):
    response = client.get("/execucoes", params={"limite": 0})
    assert response.status_code == 422
    _validar(response, "/execucoes")


def test_detalhe_segue_o_contrato(client, fabrica):
    for execucao in _popular(fabrica):
        response = client.get(f"/execucoes/{execucao.execucao_id}")
        assert response.status_code == 200
        _validar(response, "/execucoes/{execucao_id}")


def test_detalhe_inexistente_segue_o_contrato(client):
    response = client.get(f"/execucoes/{uuid.uuid4()}")
    assert response.status_code == 404
    _validar(response, "/execucoes/{execucao_id}")


def test_detalhe_12x4_com_3_tentativas_em_menos_de_300_ms(client, fabrica):
    execucao = fabrica.execucao_12x4_com_3_tentativas(
        passos_por_tentativa=150, leituras_por_tentativa=600
    )
    url = f"/execucoes/{execucao.execucao_id}"
    client.get(url)  # aquece conexão e caches do SQLAlchemy

    tempos = []
    for _ in range(5):
        inicio = time.perf_counter()
        response = client.get(url)
        tempos.append(time.perf_counter() - inicio)
        assert response.status_code == 200

    corpo = response.json()
    assert len(corpo["tentativas"]) == 3
    assert len(corpo["trajeto"]) == 450
    assert sorted(tempos)[len(tempos) // 2] < LIMITE_DETALHE_S
