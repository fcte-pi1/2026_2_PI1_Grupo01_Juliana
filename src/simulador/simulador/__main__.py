"""Linha de comando do simulador (`python -m simulador`)."""

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path

SAIDAS = ("stdout", "pty", "http")


def criar_parser() -> argparse.ArgumentParser:
    """Monta o parser com as opções do simulador."""
    parser = argparse.ArgumentParser(
        prog="python -m simulador",
        description="Faz o papel do Micromouse e emite a telemetria do contrato v1.",
    )
    parser.add_argument("roteiro", type=Path, help="arquivo JSON com o roteiro do cenário")
    parser.add_argument(
        "--saida",
        choices=SAIDAS,
        default="stdout",
        help="para onde as linhas vão (padrão: stdout)",
    )
    parser.add_argument("--url", help="URL da API, usada com --saida http")
    parser.add_argument(
        "--acelerar",
        type=float,
        default=1.0,
        metavar="N",
        help="divide as esperas e o t_ms por N (padrão: 1)",
    )
    parser.add_argument(
        "--sem-espera",
        action="store_true",
        help="emite tudo de uma vez, sem dormir",
    )
    parser.add_argument(
        "--taxa-tel",
        type=float,
        default=5.0,
        metavar="HZ",
        help="taxa da tel em movimento, de 1 a 20 (padrão: 5)",
    )
    parser.add_argument(
        "--boot",
        type=int,
        metavar="N",
        help="força o boot inicial em vez do salvo em .simulador/boot",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    """Ponto de entrada; devolve o código de saída."""
    criar_parser().parse_args(argv)
    print("simulador: execução do roteiro ainda não implementada", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
