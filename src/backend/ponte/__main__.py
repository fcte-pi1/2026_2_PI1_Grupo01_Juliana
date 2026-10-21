"""Ponto de entrada: `python -m ponte`."""

import argparse
import sys
from pathlib import Path

from ponte.config import ConfigPonte


class ErroConfig(Exception):
    """Configuração inválida passada pela linha de comando ou pelo ambiente."""


def montar_config(argv: list[str] | None = None, env_file: str | None = ".env") -> ConfigPonte:
    """Lê o ambiente e o `.env` e aplica as opções; a opção sempre vence a variável."""
    parser = argparse.ArgumentParser(prog="python -m ponte", description=__doc__)
    parser.add_argument("--url", help="URL da API (API_URL)")
    parser.add_argument("--porta", help="porta serial (PONTE_PORTA_SERIAL)")
    parser.add_argument("--fila", type=int, help="máximo de linhas na fila (PONTE_FILA_MAXIMA)")
    parser.add_argument("--log-dir", type=Path, help="pasta do log (PONTE_LOG_DIR)")
    args = parser.parse_args(argv)

    config = ConfigPonte(_env_file=env_file)
    opcoes = {
        "api_url": args.url,
        "porta_serial": args.porta,
        "fila_maxima": args.fila,
        "log_dir": args.log_dir,
    }
    config = config.model_copy(update={k: v for k, v in opcoes.items() if v is not None})
    if config.fila_maxima < 1:
        raise ErroConfig(f"a fila precisa ter pelo menos 1 linha (recebido {config.fila_maxima})")
    return config


def main(argv: list[str] | None = None) -> int:
    try:
        montar_config(argv)
    except ErroConfig as erro:
        print(f"ponte: {erro}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
