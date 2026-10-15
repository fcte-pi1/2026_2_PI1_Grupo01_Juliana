"""Leitura do openapi.yaml e validação de respostas reais contra ele."""

from pathlib import Path

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
        (Path(__file__).resolve().parents[1] / "openapi.yaml").read_text(encoding="utf-8")
    )
)


def validador(schema: dict) -> OAS30Validator:
    """Validador do esquema com os components do contrato."""
    return OAS30Validator(
        {**schema, "components": OPENAPI["components"]},
        format_checker=OAS30Validator.FORMAT_CHECKER,
    )


def validar_resposta(response, caminho: str, metodo: str = "get") -> None:
    """Valida o corpo e os cabeçalhos da resposta contra o openapi.yaml."""
    resposta = OPENAPI["paths"][caminho][metodo]["responses"][str(response.status_code)]
    if "$ref" in resposta:
        resposta = OPENAPI["components"]["responses"][resposta["$ref"].rsplit("/", 1)[-1]]
    schema = resposta["content"]["application/json"]["schema"]
    validador(schema).validate(response.json())
    for cabecalho in resposta.get("headers", {}):
        assert cabecalho in response.headers
