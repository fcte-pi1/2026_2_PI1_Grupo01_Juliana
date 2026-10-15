"""Leitura do openapi.yaml para validar respostas e frames nos testes."""

from pathlib import Path

import yaml
from openapi_schema_validator import OAS30Validator


def nulo_explicito(no):
    """Lê `allOf: [$ref] + nullable: true` como "o $ref ou null".

    É a intenção do contrato, mas a leitura estrita da OAS 3.0.3 aplica o
    nullable só ao type do próprio nó, e o validador recusaria o null.
    """
    if isinstance(no, list):
        return [nulo_explicito(item) for item in no]
    if not isinstance(no, dict):
        return no
    no = {chave: nulo_explicito(valor) for chave, valor in no.items()}
    if no.get("nullable") and "allOf" in no and "type" not in no:
        resto = {k: v for k, v in no.items() if k not in ("allOf", "nullable")}
        return {**resto, "anyOf": [{"allOf": no["allOf"]}, {"enum": [None]}]}
    return no


OPENAPI = nulo_explicito(
    yaml.safe_load(
        (Path(__file__).resolve().parents[2] / "openapi.yaml").read_text(encoding="utf-8")
    )
)


def validar_contra(nome_do_schema: str, payload) -> None:
    """Valida o payload contra um schema de components.schemas."""
    schema = OPENAPI["components"]["schemas"][nome_do_schema]
    validador = OAS30Validator(
        {**schema, "components": OPENAPI["components"]},
        format_checker=OAS30Validator.FORMAT_CHECKER,
    )
    validador.validate(payload)