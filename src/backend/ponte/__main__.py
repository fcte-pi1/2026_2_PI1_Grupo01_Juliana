"""Lê a serial (ou um arquivo) e encaminha cada linha para a API.

    python -m ponte --url http://127.0.0.1:8000 --arquivo captura.jsonl
    python -m ponte --url http://127.0.0.1:8000 --porta /dev/ttyUSB0

A porta serial precisa do pyserial (`pip install pyserial`). Sem --porta e sem
--arquivo, lê o stdin.
"""

from __future__ import annotations

import argparse
import sys
from collections.abc import Iterator

import httpx

from ponte.encaminhar import executar


def _linhas_da_serial(porta: str, baud: int) -> Iterator[str]:
    try:
        import serial
    except ImportError as erro:
        raise SystemExit("para --porta, instale pyserial") from erro
    with serial.Serial(porta, baud, timeout=1) as conexao:
        while True:
            bruta = conexao.readline()
            if bruta:
                yield bruta.decode("utf-8", errors="replace")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Ponte serial do Micromouse (BACK-08)")
    parser.add_argument("--url", default="http://127.0.0.1:8000")
    parser.add_argument("--porta", help="porta serial da ESP32, ex. /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--arquivo", help="linhas já capturadas, em vez da serial")
    args = parser.parse_args(argv)

    if args.porta and args.arquivo:
        parser.error("use só uma origem: --porta ou --arquivo")

    with httpx.Client(base_url=args.url, timeout=5.0) as cliente:
        if args.porta:
            return 0 if executar(_linhas_da_serial(args.porta, args.baud), sys.stdout, cliente) >= 0 else 1
        if args.arquivo:
            with open(args.arquivo, encoding="utf-8") as origem:
                executar(origem, sys.stdout, cliente)
            return 0
        executar(sys.stdin, sys.stdout, cliente)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
