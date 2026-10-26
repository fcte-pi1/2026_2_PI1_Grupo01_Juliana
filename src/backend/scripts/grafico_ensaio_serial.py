"""Gráfico de mensagens por segundo do ensaio serial (teste 4.16).

Plota, a partir do CSV por janela gravado por `scripts.ensaio_serial conferir --csv`,
as enviadas pelo simulador, as recebidas pela ponte (RX) e as gravadas no banco, em
mensagens por segundo, para mostrar que não houve buraco ao longo do ensaio. O CSV não
traz a janela da tentativa, então ela não é marcada.

Uso, a partir de src/backend (precisa do extra graficos):
    uv run --extra graficos python -m scripts.grafico_ensaio_serial janelas.csv ensaio.png
"""

import argparse
import csv
from pathlib import Path

SERIES = (("enviadas", "enviadas pelo simulador"), ("rx", "RX na ponte"), ("gravadas", "gravadas"))


def ler_janelas(caminho: Path) -> tuple[list[float], dict[str, list[float]]]:
    """Início de cada janela em minutos e mensagens por segundo de cada série.

    A duração da janela sai da diferença entre os inícios das duas primeiras; com uma
    janela só, vale 10 s, como em scripts.ensaio_serial.
    """
    with open(caminho, newline="", encoding="utf-8") as arquivo:
        linhas = list(csv.DictReader(arquivo))
    inicios = [float(linha["janela_inicio_s"]) for linha in linhas]
    janela_s = inicios[1] - inicios[0] if len(inicios) > 1 else 10.0
    taxas = {coluna: [float(linha[coluna]) / janela_s for linha in linhas] for coluna, _ in SERIES}
    return [inicio / 60 for inicio in inicios], taxas


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("csv", type=Path, help="CSV gravado por scripts.ensaio_serial conferir")
    parser.add_argument("png", type=Path, help="arquivo PNG de saída")
    args = parser.parse_args()

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    minutos, taxas = ler_janelas(args.csv)
    if not minutos:
        parser.error("o CSV não tem nenhuma janela")

    figura, eixo = plt.subplots(figsize=(10, 5))
    for (coluna, rotulo), estilo in zip(SERIES, ("-", "--", ":"), strict=True):
        eixo.step(minutos, taxas[coluna], where="post", linestyle=estilo, label=rotulo)
    eixo.set_xlabel("tempo desde o primeiro RX (min)")
    eixo.set_ylabel("mensagens por segundo")
    eixo.set_ylim(bottom=0)
    eixo.set_title("Ensaio serial: mensagens por segundo em janelas de 10 s")
    eixo.grid(alpha=0.3)
    eixo.legend(loc="lower center")
    figura.tight_layout()
    figura.savefig(args.png, dpi=150)
    print(f"gráfico gravado em {args.png}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
