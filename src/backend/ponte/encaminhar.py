"""Encaminha cada linha do robô para a API e escreve os comandos de volta.

A linha entra crua, como saiu da serial. A resposta de POST /telemetria traz
`comandos`; cada um volta como um JSON por linha. Um 422 também pode trazer
comandos, e a ponte os escreve do mesmo jeito. Linha em branco ou começada
com "#" (depuração do firmware) não é enviada.

Se o POST não chega (rede) ou a API responde 5xx, a linha fica numa fila em
memória e sai de novo, na mesma ordem, quando a API volta. A fila guarda no
máximo 100 linhas; acima disso, a mais antiga é descartada. O instante
`recebido_em` é o da leitura, e se mantém no reenvio.
"""

from __future__ import annotations

import json
import logging
from collections import deque
from collections.abc import Iterable, Mapping
from datetime import UTC, datetime
from typing import Protocol, TextIO

FILA_MAXIMA = 100

log = logging.getLogger(__name__)


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


class EscritaSerial(Protocol):
    def write(self, dados: bytes) -> int: ...

    def flush(self) -> None: ...


class SaidaSerial:
    """Escreve cada comando na mesma serial de onde a linha foi lida."""

    def __init__(self, conexao: EscritaSerial) -> None:
        self._conexao = conexao

    def write(self, texto: str) -> int:
        dados = texto.encode("utf-8")
        escrito = self._conexao.write(dados)
        self._conexao.flush()
        return escrito


def executar(
    origem: Iterable[str],
    destino: TextIO,
    cliente: ClienteHttp,
    *,
    agora: datetime | None = None,
    fila_maxima: int = FILA_MAXIMA,
    pendentes: deque[tuple[str, datetime]] | None = None,
) -> int:
    """Encaminha as linhas de origem e escreve os comandos em destino.

    Devolve quantas a API aceitou (status abaixo de 500). As que ficaram na
    fila continuam em `pendentes`, para o processo reenviar na próxima linha.
    """
    fila = pendentes if pendentes is not None else deque()
    enviadas = 0

    def esvaziar() -> None:
        nonlocal enviadas
        while fila:
            linha, recebido_em = fila[0]
            try:
                status, comandos = encaminhar(cliente, linha, recebido_em)
            except AssertionError:
                raise
            except Exception as erro:
                log.warning("a API não respondeu (%s); %d linhas na fila", erro, len(fila))
                return
            if status >= 500:
                log.warning("a API respondeu %d; %d linhas na fila", status, len(fila))
                return
            fila.popleft()
            enviadas += 1
            for comando in comandos:
                destino.write(json.dumps(comando, separators=(",", ":")) + "\n")

    for bruta in origem:
        linha = normalizar(bruta)
        if linha is None:
            continue
        if len(fila) >= fila_maxima:
            fila.popleft()
            log.warning("fila cheia (%d linhas): descartada a mais antiga", fila_maxima)
        fila.append((linha, agora or datetime.now(UTC)))
        esvaziar()
    return enviadas
