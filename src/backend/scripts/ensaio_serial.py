"""Ensaio de 10 min pela serial com o simulador e a ponte (caso 4.16).

Lê o stderr do simulador e o log de auditoria da ponte para conferir a cadeia
simulador, ponte, API e banco.
"""

import json
import re
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

from contrato.telemetria import Comando
from pydantic import ValidationError

PREFIXO_ENVIADA = "simulador: enviada: "
TIPOS_PONTE = ("RX", "TX", "API", "CONEXAO", "FILA")
RESPOSTA_API = re.compile(r"(\d{3}) comandos=\d+")


@dataclass(frozen=True)
class EventoPonte:
    """Uma linha do log de auditoria da ponte."""

    hora: datetime
    tipo: str
    conteudo: str


def ler_log_simulador(caminho: Path | str) -> list[str]:
    """JSON de cada linha `simulador: enviada: ...`, em ordem; ignora as demais."""
    enviadas = []
    with Path(caminho).open(encoding="utf-8") as arquivo:
        for linha in arquivo:
            linha = linha.rstrip("\n")
            if linha.startswith(PREFIXO_ENVIADA):
                enviadas.append(linha.removeprefix(PREFIXO_ENVIADA))
    return enviadas


def ler_log_ponte(caminho: Path | str) -> list[EventoPonte]:
    """Eventos `<hora ISO> <TIPO> <conteudo>` do log da ponte, em ordem."""
    eventos = []
    with Path(caminho).open(encoding="utf-8") as arquivo:
        for numero, linha in enumerate(arquivo, start=1):
            linha = linha.rstrip("\n")
            if not linha:
                continue
            partes = linha.split(" ", 2)
            if len(partes) < 2 or partes[1] not in TIPOS_PONTE:
                raise ValueError(f"linha {numero} fora do formato: {linha!r}")
            hora = datetime.fromisoformat(partes[0])
            if hora.tzinfo is None:
                raise ValueError(f"linha {numero} sem fuso: {linha!r}")
            conteudo = partes[2] if len(partes) == 3 else ""
            eventos.append(EventoPonte(hora=hora, tipo=partes[1], conteudo=conteudo))
    return eventos


@dataclass(frozen=True)
class ResultadoPonte:
    """Conferência da serial (enviadas contra RX) e da ponte (API, FILA e TX)."""

    enviadas: int
    rx: int
    api_200: int
    api_outro_status: int
    api_falhou: int
    fila_descartada: int
    tx: list[str]
    tx_invalidos: list[str]
    problemas: list[str]

    @property
    def aprovado(self) -> bool:
        return not self.problemas


def _comando_valido(texto: str) -> bool:
    """Se o TX é o JSON de um `Comando` do contrato, sem campo a mais."""
    try:
        dado = json.loads(texto)
        comando = Comando.model_validate(dado, strict=True)
    except (ValueError, ValidationError):
        return False
    return comando.model_dump() == dado


def conferir_ponte(enviadas: list[str], eventos: list[EventoPonte]) -> ResultadoPonte:
    """Toda linha enviada, menos a primeira (descartada pela ponte), chegou e teve 200."""
    problemas = []
    rx = [e.conteudo for e in eventos if e.tipo == "RX"]
    api = [e.conteudo for e in eventos if e.tipo == "API"]
    tx = [e.conteudo for e in eventos if e.tipo == "TX"]
    fila = [e.conteudo for e in eventos if e.tipo == "FILA"]

    if len(enviadas) != len(rx) + 1:
        problemas.append(f"enviadas ({len(enviadas)}) diferente de RX + 1 ({len(rx) + 1})")
    esperadas = enviadas[1:]
    for indice, (enviada, recebida) in enumerate(zip(esperadas, rx, strict=False)):
        if enviada != recebida:
            problemas.append(
                f"RX {indice + 1} diferente da enviada {indice + 2}: {recebida!r} e {enviada!r}"
            )
            break

    status = [m.group(1) for c in api if (m := RESPOSTA_API.fullmatch(c))]
    api_200 = status.count("200")
    falhou = [c for c in api if c.startswith("falhou")]
    outros_api = [c for c in api if not RESPOSTA_API.fullmatch(c) and not c.startswith("falhou")]
    if len(status) != len(rx):
        problemas.append(f"respostas da API ({len(status)}) diferente de RX ({len(rx)})")
    if len(status) != api_200:
        outros = sorted({s for s in status if s != "200"})
        problemas.append(f"{len(status) - api_200} respostas da API com status {', '.join(outros)}")
    if falhou:
        problemas.append(f"{len(falhou)} API falhou, a primeira: {falhou[0]}")
    for conteudo in outros_api:
        problemas.append(f"API {conteudo}")
    if fila:
        problemas.append(f"{len(fila)} FILA descartada, a primeira: {fila[0]}")
    tx_invalidos = [t for t in tx if not _comando_valido(t)]
    for texto in tx_invalidos:
        problemas.append(f"TX fora do contrato: {texto}")

    return ResultadoPonte(
        enviadas=len(enviadas),
        rx=len(rx),
        api_200=api_200,
        api_outro_status=len(status) - api_200,
        api_falhou=len(falhou),
        fila_descartada=len(fila),
        tx=tx,
        tx_invalidos=tx_invalidos,
        problemas=problemas,
    )
