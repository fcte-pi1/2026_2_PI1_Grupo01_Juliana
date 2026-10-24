"""Encaminha cada linha do robô para a API e escreve os comandos de volta.

A linha entra crua, como saiu da serial. A resposta de POST /telemetria traz
`comandos`; cada um volta como um JSON por linha. Um 422 também pode trazer
comandos, e a ponte os escreve do mesmo jeito. Linha em branco ou começada
com "#" (depuração do firmware) não é enviada.
"""

from __future__ import annotations

import json
from collections.abc import Iterable, Mapping
from datetime import UTC, datetime
from typing import Protocol, TextIO


class RespostaHttp(Protocol):
    status_code: int

    def json(self) -> object: ...


class ClienteHttp(Protocol):
    def post(self, caminho: str, *, json: Mapping[str, object]) -> RespostaHttp: ...


def normalizar(linha: str) -> str | None:
    """Devolve a linha com \\n final, ou None se ela não for telemetria."""
    corpo = linha[:-1] if linha.endswith("\n") else linha
    corpo = corpo.rstrip("\r")
    if not corpo.strip() or corpo.lstrip().startswith("#"):
        return None
    return corpo + "\n"


def comandos_da(resposta: RespostaHttp) -> list[dict[str, object]]:
    try:
        corpo = resposta.json()
    except (ValueError, TypeError):
        return []
    if not isinstance(corpo, dict):
        return []
    comandos = corpo.get("comandos", [])
    if not isinstance(comandos, list):
        return []
    return [item for item in comandos if isinstance(item, dict)]


def encaminhar(
    cliente: ClienteHttp,
    linha: str,
    recebido_em: datetime,
    *,
    caminho: str = "/telemetria",
) -> tuple[int, list[dict[str, object]]]:
    """POST da linha. Devolve o status HTTP e os comandos para a serial."""
    resposta = cliente.post(
        caminho,
        json={"linha": linha, "recebido_em": recebido_em.isoformat()},
    )
    return resposta.status_code, comandos_da(resposta)


def executar(
    origem: Iterable[str],
    destino: TextIO,
    cliente: ClienteHttp,
    *,
    agora: datetime | None = None,
) -> int:
    """Encaminha as linhas de origem e escreve os comandos em destino. Devolve quantas enviou."""
    enviadas = 0
    for bruta in origem:
        linha = normalizar(bruta)
        if linha is None:
            continue
        _status, comandos = encaminhar(cliente, linha, agora or datetime.now(UTC))
        for comando in comandos:
            destino.write(json.dumps(comando, separators=(",", ":")) + "\n")
        enviadas += 1
    return enviadas
