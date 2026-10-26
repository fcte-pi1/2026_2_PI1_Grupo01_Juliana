"""Ensaio de 10 min pela serial com o simulador e a ponte (caso 4.16).

Lê o stderr do simulador e o log de auditoria da ponte para conferir a cadeia
simulador, ponte, API e banco.
"""

import argparse
import json
import math
import re
import sys
import time
import uuid
from collections import Counter
from collections.abc import Callable
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from pathlib import Path
from typing import TYPE_CHECKING

from contrato.telemetria import Comando
from pydantic import ValidationError
from sqlalchemy import func, select
from sqlalchemy.orm import Session

if TYPE_CHECKING:
    import httpx

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


FRONTEIRA = timedelta(milliseconds=50)
ITENS_HEALTH_CHECK = 8


@dataclass(frozen=True)
class ResultadoBanco:
    """Conferência no banco do que chegou com a tentativa aberta."""

    execucao_id: uuid.UUID | None
    motivo_encerramento: str | None
    antes: dict[str, int]
    dentro: dict[str, int]
    depois: dict[str, int]
    gravados: dict[str, int]
    fronteira: list[EventoPonte]
    problemas: list[str]

    @property
    def aprovado(self) -> bool:
        return not self.problemas


def _mensagem(evento: EventoPonte) -> dict:
    try:
        dado = json.loads(evento.conteudo)
    except ValueError:
        return {}
    return dado if isinstance(dado, dict) else {}


def _subsequencia(exigidos: list[tuple[int, int]], gravados: list[tuple[int, int]]) -> int | None:
    """Índice do primeiro exigido que não aparece, em ordem, nos gravados."""
    restantes = iter(gravados)
    for indice, celula in enumerate(exigidos):
        if celula not in restantes:
            return indice
    return None


def conferir_banco(sessao: Session, eventos: list[EventoPonte], boot: int) -> ResultadoBanco:
    """Todo `tel` e `passo` RX dentro da janela da tentativa do `boot` foi gravado.

    O backend numera `passo_trajeto.seq` por conta própria (1, 2, ...) e só
    regrava as paredes quando a célula repete; por isso os passos são
    conferidos pela célula, na ordem, e não pelo `seq` da mensagem.
    """
    from app.models import (
        HealthCheck,
        HealthCheckItem,
        LeituraTelemetria,
        PassoTrajeto,
        Tentativa,
    )

    problemas = []
    tentativas = sessao.scalars(
        select(Tentativa).join(HealthCheck).where(HealthCheck.boot == boot)
    ).all()
    if not tentativas:
        problemas.append(f"nenhuma tentativa com health-check do boot {boot}")
        return ResultadoBanco(None, None, {}, {}, {}, {}, [], problemas)
    if len(tentativas) > 1:
        problemas.append(f"{len(tentativas)} tentativas com health-check do boot {boot}")
    tentativa = tentativas[0]
    inicio, fim = tentativa.iniciada_em, tentativa.encerrada_em
    if fim is None:
        problemas.append("tentativa ainda aberta")

    antes: Counter[str] = Counter()
    dentro: Counter[str] = Counter()
    depois: Counter[str] = Counter()
    fronteira = []
    passos_exigidos: list[tuple[int, int]] = []
    for evento in (e for e in eventos if e.tipo == "RX"):
        dado = _mensagem(evento)
        tipo = dado.get("tipo", "?")
        perto = abs(evento.hora - inicio) < FRONTEIRA
        perto = perto or (fim is not None and abs(evento.hora - fim) < FRONTEIRA)
        if perto:
            fronteira.append(evento)
        elif evento.hora < inicio:
            antes[tipo] += 1
        elif fim is not None and evento.hora > fim:
            depois[tipo] += 1
        else:
            dentro[tipo] += 1
            celula = (dado.get("x"), dado.get("y"))
            if tipo == "passo" and (not passos_exigidos or passos_exigidos[-1] != celula):
                passos_exigidos.append(celula)

    passos = sessao.execute(
        select(PassoTrajeto.x, PassoTrajeto.y)
        .where(PassoTrajeto.execucao_id == tentativa.execucao_id)
        .order_by(PassoTrajeto.seq)
    ).all()
    leituras = sessao.scalar(
        select(func.count())
        .select_from(LeituraTelemetria)
        .where(LeituraTelemetria.tentativa_id == tentativa.tentativa_id)
    )
    itens = sessao.scalar(
        select(func.count())
        .select_from(HealthCheckItem)
        .where(HealthCheckItem.hc_id == tentativa.hc_id)
    )

    faltando = _subsequencia(passos_exigidos, [tuple(p) for p in passos])
    if faltando is not None:
        problemas.append(
            f"passo {faltando + 1} da janela não gravado: célula {passos_exigidos[faltando]}"
        )
    tel_fronteira = sum(1 for e in fronteira if _mensagem(e).get("tipo") == "tel")
    if not dentro["tel"] <= leituras <= dentro["tel"] + tel_fronteira:
        problemas.append(
            f"leituras gravadas ({leituras}) diferente de tel RX na janela ({dentro['tel']}"
            f", mais {tel_fronteira} na fronteira)"
        )
    if itens != ITENS_HEALTH_CHECK:
        problemas.append(f"health-check com {itens} itens, esperado {ITENS_HEALTH_CHECK}")

    return ResultadoBanco(
        execucao_id=tentativa.execucao_id,
        motivo_encerramento=tentativa.falha.motivo if tentativa.falha else None,
        antes=dict(antes),
        dentro=dict(dentro),
        depois=dict(depois),
        gravados={"tel": leituras, "passo": len(passos), "hc_item": itens},
        fronteira=fronteira,
        problemas=problemas,
    )


INTERVALO_CONSULTA_S = 0.1


class FalhaAbertura(RuntimeError):
    """A execução não abriu: sem health-check novo no prazo ou recusa da API."""


@dataclass(frozen=True)
class ExecucaoAberta:
    execucao_id: uuid.UUID
    tentativa_id: uuid.UUID
    aberta_em: datetime


def _health_check_novo(cliente: "httpx.Client", inicio: datetime) -> bool:
    resposta = cliente.get("/health-check/ultimo")
    if resposta.status_code == 204:
        return False
    resposta.raise_for_status()
    concluido_em = resposta.json().get("concluido_em")
    return concluido_em is not None and datetime.fromisoformat(concluido_em) > inicio


def abrir_execucao(
    cliente: "httpx.Client",
    labirinto: str,
    timeout_s: float,
    inicio: datetime,
    dormir: Callable[[float], None] = time.sleep,
) -> ExecucaoAberta:
    """Espera um health-check concluído depois de `inicio` e abre a execução.

    Consulta a cada 100 ms, até `timeout_s` (contado em consultas, para o teste
    não depender do relógio).
    """
    consultas = max(1, math.ceil(timeout_s / INTERVALO_CONSULTA_S))
    for numero in range(consultas):
        if _health_check_novo(cliente, inicio):
            break
        if numero < consultas - 1:
            dormir(INTERVALO_CONSULTA_S)
    else:
        raise FalhaAbertura(f"nenhum health-check concluído depois do início em {timeout_s:g} s")

    resposta = cliente.post("/execucoes", json={"tipo_labirinto": labirinto})
    if resposta.status_code == 409:
        raise FalhaAbertura(f"execução recusada (409): {resposta.json().get('motivo')}")
    resposta.raise_for_status()
    corpo = resposta.json()
    return ExecucaoAberta(
        execucao_id=uuid.UUID(corpo["execucao_id"]),
        tentativa_id=uuid.UUID(corpo["tentativa_id"]),
        aberta_em=datetime.now(UTC),
    )


def _comando_abrir(args: argparse.Namespace) -> int:
    import httpx

    inicio = datetime.now(UTC)
    with httpx.Client(base_url=args.url, timeout=5) as cliente:
        try:
            aberta = abrir_execucao(cliente, args.labirinto, args.timeout, inicio)
        except (FalhaAbertura, httpx.HTTPError) as erro:
            print(f"abrir: {erro}", file=sys.stderr)
            return 1
    print(f"execucao_id={aberta.execucao_id}")
    print(f"tentativa_id={aberta.tentativa_id}")
    print(f"aberta_em={aberta.aberta_em.isoformat(timespec='milliseconds')}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    comandos = parser.add_subparsers(dest="comando", required=True)
    abrir = comandos.add_parser("abrir", help="abre a execução logo depois do health-check")
    abrir.add_argument("--url", default="http://localhost:8000", help="URL base da API")
    abrir.add_argument("--labirinto", default="4x4", choices=("4x4", "8x4", "12x4"))
    abrir.add_argument("--timeout", type=float, default=60, help="espera máxima em segundos")
    abrir.set_defaults(executar=_comando_abrir)
    args = parser.parse_args(argv)
    return args.executar(args)


if __name__ == "__main__":
    sys.exit(main())
