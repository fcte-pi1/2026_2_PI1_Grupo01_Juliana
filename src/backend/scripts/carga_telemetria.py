"""Carga de telemetria contra a API no ar (RNF-B11).

Envia `tel` a POST /telemetria no ritmo pedido e imprime p50, p95, máx e o
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
from datetime import UTC, datetime

import httpx

P95_MAXIMO_S = 0.100
ATRASO_MAXIMO_S = 1.0


def _p95(tempos: list[float]) -> float:
    """Percentil 95 pelo método do posto mais próximo."""
    ordenados = sorted(tempos)
    return ordenados[math.ceil(0.95 * len(ordenados)) - 1]


def _tel(boot: int, seq: int, t_ms: int) -> str:
    """Linha `tel` válida em (0, 0), dentro de qualquer labirinto."""
    mensagem = {
        "v": 1,
        "boot": boot,
        "seq": seq,
        "t_ms": t_ms,
        "tipo": "tel",
        "estado": "running",
        "x": 0,
        "y": 0,
        "rumo": "N",
        "bat_mv": 7800,
        "vel_mm_s": 200,
        "eixo_longo": None,
    }
    return json.dumps(mensagem, separators=(",", ":"))


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
                "linha": _tel(boot, seq, round(seq * intervalo * 1000)),
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
