"""Gráfico da latência por mensagem a partir do CSV do teste de carga (RNF-B11).

Plota a latência de cada mensagem ao longo da carga, o limite de 100 ms do p95 e o p95
por janela de 10 s, para mostrar se há atraso acumulado. Mensagens com erro de rede
(latência vazia no CSV) ficam de fora.

Uso, a partir de src/backend (precisa do extra graficos):
    uv run --extra graficos python -m scripts.grafico_carga carga.csv carga.png
"""

import argparse
import csv
import math
from pathlib import Path

JANELA_S = 10
LIMITE_P95_MS = 100.0


def _p95(tempos: list[float]) -> float:
    """Percentil 95 pelo posto mais próximo, como em scripts.carga_telemetria.

    Fica duplicado aqui para o gráfico rodar só com o extra graficos, sem httpx.
    """
    ordenados = sorted(tempos)
    return ordenados[math.ceil(0.95 * len(ordenados)) - 1]


def ler_latencias(caminho: Path) -> list[tuple[float, float]]:
    """Pares (instante_s, latencia_ms) das mensagens com latência medida."""
    with open(caminho, newline="", encoding="utf-8") as arquivo:
        return [
            (float(linha["instante_s"]), float(linha["latencia_ms"]))
            for linha in csv.DictReader(arquivo)
            if linha["latencia_ms"]
        ]


def p95_por_janela(pontos: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """Pares (meio da janela em s, p95 em ms) das janelas de JANELA_S segundos."""
    janelas: dict[int, list[float]] = {}
    for instante, latencia in pontos:
        janelas.setdefault(int(instante // JANELA_S), []).append(latencia)
    return [((i + 0.5) * JANELA_S, _p95(janelas[i])) for i in sorted(janelas)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, help="CSV gravado por scripts.carga_telemetria")
    parser.add_argument("png", type=Path, help="arquivo PNG de saída")
    args = parser.parse_args()

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    pontos = ler_latencias(args.csv)
    if not pontos:
        parser.error("o CSV não tem nenhuma latência medida")
    instantes = [p[0] for p in pontos]
    latencias = [p[1] for p in pontos]
    janelas = p95_por_janela(pontos)

    figura, eixo = plt.subplots(figsize=(10, 5))
    eixo.scatter(instantes, latencias, s=8, alpha=0.6, label="latência por mensagem")
    eixo.plot(
        [j[0] for j in janelas],
        [j[1] for j in janelas],
        marker="o",
        color="tab:orange",
        label=f"p95 por janela de {JANELA_S} s",
    )
    eixo.axhline(LIMITE_P95_MS, color="tab:red", linestyle="--", label="limite p95 (100 ms)")
    eixo.set_xlabel("tempo desde o início (s)")
    eixo.set_ylabel("latência (ms)")
    eixo.set_ylim(bottom=0, top=max(LIMITE_P95_MS, max(latencias)) * 1.1)
    eixo.set_title(f"Carga de telemetria: p95 total de {_p95(latencias):.1f} ms")
    eixo.grid(alpha=0.3)
    eixo.legend(loc="center right")
    figura.tight_layout()
    figura.savefig(args.png, dpi=150)
    print(f"gráfico gravado em {args.png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
