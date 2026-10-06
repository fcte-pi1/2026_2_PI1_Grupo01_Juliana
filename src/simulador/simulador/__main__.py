"""Linha de comando do simulador (`python -m simulador`)."""

import argparse
import logging
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

from simulador.emissor import TAXA_TEL_MAXIMA_HZ, TAXA_TEL_MINIMA_HZ, emitir
from simulador.gerador import Opcoes, gerar
from simulador.roteiro import RoteiroInvalido, carregar
from simulador.saidas import PtyIndisponivel, SaidaPty

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


def _configurar_log() -> logging.Logger:
    """Log em stderr, para o stdout ficar só com as linhas (FR-10)."""
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(logging.Formatter("simulador: %(message)s"))
    log = logging.getLogger("simulador")
    log.handlers = [handler]
    log.setLevel(logging.INFO)
    log.propagate = False
    return log


def main(argv: Sequence[str] | None = None) -> int:
    """Ponto de entrada; devolve o código de saída."""
    args = criar_parser().parse_args(argv)
    log = _configurar_log()

    if not TAXA_TEL_MINIMA_HZ <= args.taxa_tel <= TAXA_TEL_MAXIMA_HZ:
        log.error(
            "--taxa-tel deve ficar entre %d e %d Hz (recebido %g)",
            TAXA_TEL_MINIMA_HZ,
            TAXA_TEL_MAXIMA_HZ,
            args.taxa_tel,
        )
        return 1
    if args.acelerar <= 0:
        log.error("--acelerar deve ser maior que 0 (recebido %g)", args.acelerar)
        return 1
    if args.saida == "http":
        log.error("--saida %s ainda não implementada", args.saida)
        return 1
    try:
        roteiro = carregar(args.roteiro)
    except RoteiroInvalido as erro:
        log.error("%s", erro)
        return 1

    opcoes = Opcoes(boot=args.boot or 0, taxa_tel_hz=args.taxa_tel)

    def rodar(escrever: Callable[[str], None]) -> int:
        return emitir(
            gerar(roteiro, opcoes), escrever, acelerar=args.acelerar, sem_espera=args.sem_espera
        )

    if args.saida == "pty":
        try:
            saida = SaidaPty(ao_receber=lambda linha: log.info("recebida: %s", linha))
        except PtyIndisponivel as erro:
            log.error("%s", erro)
            return 1
        with saida:
            log.info("porta serial virtual: %s (esperando alguém abrir)", saida.caminho)
            saida.aguardar_conexao()
            log.info("porta aberta, começando o roteiro")
            enviadas = rodar(saida.escrever)
    else:

        def escrever(linha: str) -> None:
            sys.stdout.write(linha)
            sys.stdout.flush()

        enviadas = rodar(escrever)
    log.info("roteiro %s terminado: %d linhas", roteiro.nome, enviadas)
    return 0


if __name__ == "__main__":
    sys.exit(main())
