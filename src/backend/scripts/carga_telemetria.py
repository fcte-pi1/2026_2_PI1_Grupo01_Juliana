"""Carga de telemetria contra a API no ar (RNF-B11).

Envia `tel` e `passo` a POST /telemetria no ritmo pedido e imprime p50, p95, máx e o
atraso acumulado. Sai com código 1 se p95 >= 100 ms, se o atraso acumulado
passar de 1 s ou se alguma resposta não for 200. Precisa de uma tentativa
aberta no banco (veja o README do backend).

Uso, a partir de src/backend:
    python -m scripts.carga_telemetria --url http://localhost:8000 --taxa 10 --duracao 60
"""

import argparse
import json
import math
import sys
import time
import uuid
from collections.abc import Callable
from dataclasses import dataclass
from datetime import UTC, datetime

import httpx
from sqlalchemy.orm import Session

P95_MAXIMO_S = 0.100
ATRASO_MAXIMO_S = 1.0
PISO_CRESCIMENTO_S = 0.010


def _p95(tempos: list[float]) -> float:
    """Percentil 95 pelo método do posto mais próximo."""
    ordenados = sorted(tempos)
    return ordenados[math.ceil(0.95 * len(ordenados)) - 1]


def sem_crescimento(tempos: list[float], janela: int) -> bool:
    """Compara o p95 das `janela` primeiras latências com o das últimas.

    Aceita o final até o dobro do inicial ou até o inicial mais 10 ms, para não
    reprovar por ruído quando as latências são de poucos milissegundos.
    """
    if len(tempos) < 2 * janela:
        raise ValueError(f"precisa de ao menos {2 * janela} latências, veio {len(tempos)}")
    inicial = _p95(tempos[:janela])
    final = _p95(tempos[-janela:])
    return final <= max(2 * inicial, inicial + PISO_CRESCIMENTO_S)


def mensagem(boot: int, seq: int, t_ms: int) -> str:
    """`tel` nos seq pares e `passo` nos ímpares, alternando entre (0, 0) e (0, 1)."""
    base = {"v": 1, "boot": boot, "seq": seq, "t_ms": t_ms}
    if seq % 2 == 0:
        campos = {
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 7800,
            "vel_mm_s": 200,
            "eixo_longo": None,
        }
    else:
        campos = {"tipo": "passo", "x": 0, "y": (seq // 2) % 2, "paredes": 9}
    return json.dumps({**base, **campos}, separators=(",", ":"))


class TentativaAbertaError(RuntimeError):
    """Já há tentativa em running: a carga gravaria nela e não na sua."""


@dataclass
class Registro:
    seq: int
    tipo: str
    instante_s: float
    latencia_s: float | None
    status: int | None


@dataclass
class ResultadoCarga:
    execucao_id: uuid.UUID
    registros: list[Registro]
    erros: int
    atraso_s: float
    p50_s: float | None
    p95_s: float | None
    max_s: float | None
    p95_inicial_s: float | None
    p95_final_s: float | None
    aprovado: bool


def _abrir_tentativa(abrir_sessao: Callable[[], Session]) -> tuple[uuid.UUID, uuid.UUID]:
    """Abre execução e tentativa da carga no labirinto 4x4; recusa se já houver tentativa aberta."""
    from app.models import ExecucaoLogica, Tentativa
    from app.repositories import Repositorio

    with abrir_sessao() as sessao:
        repositorio = Repositorio(sessao)
        if repositorio.buscar_tentativa_aberta() is not None:
            raise TentativaAbertaError("já há tentativa aberta no banco")
        agora = datetime.now(UTC)
        execucao = ExecucaoLogica(
            execucao_id=uuid.uuid4(),
            labirinto_id=repositorio.labirinto_id_por_tipo("4x4"),
            status="em_andamento",
            tentativas_usadas=1,
            iniciada_em=agora,
        )
        repositorio.salvar(execucao)
        tentativa = Tentativa(
            tentativa_id=uuid.uuid4(),
            execucao_id=execucao.execucao_id,
            attempt_index=1,
            status="running",
            tipo_inicio="nova",
            iniciada_em=agora,
        )
        repositorio.salvar(tentativa)
        sessao.commit()
        return execucao.execucao_id, tentativa.tentativa_id


def _fechar_tentativa(
    abrir_sessao: Callable[[], Session], execucao_id: uuid.UUID, tentativa_id: uuid.UUID
) -> None:
    """Marca a tentativa como failed e a execução como cancelada, se ainda abertas."""
    from app.models import ExecucaoLogica, Tentativa

    with abrir_sessao() as sessao:
        agora = datetime.now(UTC)
        tentativa = sessao.get(Tentativa, tentativa_id, populate_existing=True)
        if tentativa is not None and tentativa.status == "running":
            tentativa.status = "failed"
            tentativa.encerrada_em = agora
        execucao = sessao.get(ExecucaoLogica, execucao_id, populate_existing=True)
        if execucao is not None and execucao.status == "em_andamento":
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
        sessao.commit()


def _enviar(cliente: httpx.Client, taxa: float, duracao: float) -> tuple[list[Registro], float]:
    """Envia `taxa * duracao` mensagens no ritmo pedido; devolve registros e maior atraso."""
    total = int(taxa * duracao)
    intervalo = 1 / taxa
    boot = int(time.time()) % 2**32
    registros: list[Registro] = []
    atraso = 0.0
    inicio = time.perf_counter()
    for seq in range(total):
        previsto = inicio + seq * intervalo
        espera = previsto - time.perf_counter()
        if espera > 0:
            time.sleep(espera)
        enviado = time.perf_counter()
        atraso = max(atraso, enviado - previsto)
        linha = mensagem(boot, seq, round(seq * intervalo * 1000))
        corpo = {"linha": linha, "recebido_em": datetime.now(UTC).isoformat()}
        tipo = "tel" if seq % 2 == 0 else "passo"
        try:
            resposta = cliente.post("/telemetria", json=corpo)
        except httpx.HTTPError as erro:
            print(f"seq {seq}: {erro}", file=sys.stderr)
            registros.append(Registro(seq, tipo, enviado - inicio, None, None))
            continue
        latencia = time.perf_counter() - enviado
        if resposta.status_code != 200:
            print(f"seq {seq}: {resposta.status_code} {resposta.text}", file=sys.stderr)
        registros.append(Registro(seq, tipo, enviado - inicio, latencia, resposta.status_code))
    return registros, atraso


def executar_carga(
    cliente: httpx.Client, abrir_sessao: Callable[[], Session], taxa: float, duracao: float
) -> ResultadoCarga:
    """Abre uma tentativa, envia a carga e fecha a tentativa em qualquer saída."""
    execucao_id, tentativa_id = _abrir_tentativa(abrir_sessao)
    try:
        registros, atraso = _enviar(cliente, taxa, duracao)
    finally:
        _fechar_tentativa(abrir_sessao, execucao_id, tentativa_id)

    latencias = [r.latencia_s for r in registros if r.latencia_s is not None]
    erros = sum(1 for r in registros if r.status != 200)
    janela = min(round(taxa * 10), len(registros) // 2)
    p50 = sorted(latencias)[len(latencias) // 2] if latencias else None
    p95 = _p95(latencias) if latencias else None
    p95_inicial = p95_final = None
    cresceu = True
    if janela > 0 and len(latencias) >= 2 * janela:
        p95_inicial = _p95(latencias[:janela])
        p95_final = _p95(latencias[-janela:])
        cresceu = not sem_crescimento(latencias, janela)
    aprovado = (
        p95 is not None
        and p95 < P95_MAXIMO_S
        and atraso <= ATRASO_MAXIMO_S
        and erros == 0
        and not cresceu
    )
    return ResultadoCarga(
        execucao_id=execucao_id,
        registros=registros,
        erros=erros,
        atraso_s=atraso,
        p50_s=p50,
        p95_s=p95,
        max_s=max(latencias) if latencias else None,
        p95_inicial_s=p95_inicial,
        p95_final_s=p95_final,
        aprovado=aprovado,
    )


def _ha_tentativa_aberta() -> bool:
    """Sem tentativa aberta, toda tel é descartada e a carga não mede a gravação."""
    from app.db import SessionLocal
    from app.repositories import Repositorio

    with SessionLocal() as sessao:
        return Repositorio(sessao).buscar_tentativa_aberta() is not None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--url", default="http://localhost:8000", help="URL base da API")
    parser.add_argument("--taxa", type=float, default=10, help="mensagens por segundo")
    parser.add_argument("--duracao", type=float, default=60, help="duração em segundos")
    args = parser.parse_args()

    if not _ha_tentativa_aberta():
        print("nenhuma tentativa aberta no DATABASE_URL: crie uma (README)", file=sys.stderr)
        return 1

    total = int(args.taxa * args.duracao)
    intervalo = 1 / args.taxa
    boot = int(time.time()) % 2**32
    tempos: list[float] = []
    erros = 0
    atraso = 0.0

    with httpx.Client(base_url=args.url, timeout=5) as cliente:
        inicio = time.perf_counter()
        for seq in range(total):
            previsto = inicio + seq * intervalo
            espera = previsto - time.perf_counter()
            if espera > 0:
                time.sleep(espera)
            enviado = time.perf_counter()
            atraso = max(atraso, enviado - previsto)
            corpo = {
                "linha": mensagem(boot, seq, round(seq * intervalo * 1000)),
                "recebido_em": datetime.now(UTC).isoformat(),
            }
            try:
                resposta = cliente.post("/telemetria", json=corpo)
            except httpx.HTTPError as erro:
                print(f"seq {seq}: {erro}", file=sys.stderr)
                erros += 1
                continue
            tempos.append(time.perf_counter() - enviado)
            if resposta.status_code != 200:
                print(f"seq {seq}: {resposta.status_code} {resposta.text}", file=sys.stderr)
                erros += 1

    if not tempos:
        print("nenhuma resposta recebida", file=sys.stderr)
        return 1

    p50 = sorted(tempos)[len(tempos) // 2]
    p95 = _p95(tempos)
    print(f"mensagens: {total} ({erros} com erro)")
    print(f"p50: {p50 * 1000:.1f} ms")
    print(f"p95: {p95 * 1000:.1f} ms")
    print(f"máx: {max(tempos) * 1000:.1f} ms")
    print(f"atraso acumulado: {atraso * 1000:.1f} ms")

    reprovado = p95 >= P95_MAXIMO_S or atraso > ATRASO_MAXIMO_S or erros > 0
    print("REPROVADO" if reprovado else "APROVADO")
    return 1 if reprovado else 0


if __name__ == "__main__":
    sys.exit(main())
