"""Ensaio de 10 min pela serial com o simulador e a ponte (caso 4.16).

Lê o stderr do simulador e o log de auditoria da ponte para conferir a cadeia
simulador, ponte, API e banco.
"""

from dataclasses import dataclass
from datetime import datetime
from pathlib import Path

PREFIXO_ENVIADA = "simulador: enviada: "
TIPOS_PONTE = ("RX", "TX", "API", "CONEXAO", "FILA")


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
