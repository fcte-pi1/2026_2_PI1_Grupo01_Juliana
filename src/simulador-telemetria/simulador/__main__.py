"""Linha de comando do simulador (`python -m simulador`)."""

import argparse
import logging
import os
import sys
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path

import httpx

from simulador.emissor import TAXA_TEL_MAXIMA_HZ, TAXA_TEL_MINIMA_HZ, emitir
from simulador.gerador import Opcoes, gerar
from simulador.interrupcao import Interrupcao
from simulador.roteiro import RoteiroInvalido, carregar
from simulador.saidas import PtyIndisponivel, SaidaHttp, SaidaPty

SAIDAS = ("stdout", "pty", "http")
SAIDA_PADRAO = "stdout"
ACELERAR_PADRAO = 1.0
TAXA_TEL_PADRAO_HZ = 5.0
TIMEOUT_HTTP_S = 2.0
ARQUIVO_BOOT = Path(".simulador/boot")  # faz o papel da NVS do robô


class ConfiguracaoInvalida(ValueError):
    """Valor de configuração que não serve, vindo do ambiente."""


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
        help="para onde as linhas vão (padrão: $SIMULADOR_SAIDA ou stdout)",
    )
    parser.add_argument("--url", help="URL da API, usada com --saida http (padrão: $API_URL)")
    parser.add_argument(
        "--acelerar",
        type=float,
        metavar="N",
        help="divide as esperas e o t_ms por N (padrão: $SIMULADOR_ACELERAR ou 1)",
    )
    parser.add_argument(
        "--sem-espera",
        action="store_true",
        help="emite tudo de uma vez, sem dormir",
    )
    parser.add_argument(
        "--taxa-tel",
        type=float,
        metavar="HZ",
        help="taxa da tel em movimento, de 1 a 20 (padrão: $SIMULADOR_TAXA_TEL_HZ ou 5)",
    )
    parser.add_argument(
        "--boot",
        type=int,
        metavar="N",
        help="força o boot inicial em vez do salvo em .simulador/boot",
    )
    return parser


def _numero(ambiente: Mapping[str, str], nome: str, padrao: float) -> float:
    """Lê um número do ambiente; sem a variável, devolve o padrão."""
    texto = ambiente.get(nome)
    if texto is None:
        return padrao
    try:
        return float(texto)
    except ValueError:
        raise ConfiguracaoInvalida(f"{nome} deve ser um número (recebido {texto!r})") from None


def aplicar_ambiente(args: argparse.Namespace, ambiente: Mapping[str, str]) -> None:
    """Preenche as opções que faltaram com as variáveis do .env; a opção vence a variável."""
    if args.saida is None:
        args.saida = ambiente.get("SIMULADOR_SAIDA", SAIDA_PADRAO)
        if args.saida not in SAIDAS:
            raise ConfiguracaoInvalida(
                f"SIMULADOR_SAIDA deve ser {', '.join(SAIDAS)} (recebido {args.saida!r})"
            )
    if args.url is None:
        args.url = ambiente.get("API_URL")
    if args.acelerar is None:
        args.acelerar = _numero(ambiente, "SIMULADOR_ACELERAR", ACELERAR_PADRAO)
    if args.taxa_tel is None:
        args.taxa_tel = _numero(ambiente, "SIMULADOR_TAXA_TEL_HZ", TAXA_TEL_PADRAO_HZ)


def reservar_boots(arquivo: Path, quantos: int, forcado: int | None = None) -> int:
    """Devolve o primeiro boot da execução e grava no arquivo o próximo livre.

    Sem `forcado`, começa no boot salvo (0 se o arquivo não existir), para não repetir
    boot entre execuções e o Deduplicador do backend não descartar os eventos.
    """
    if forcado is not None:
        primeiro = forcado
    else:
        try:
            primeiro = int(arquivo.read_text())
        except FileNotFoundError:
            primeiro = 0
        except ValueError:
            raise ConfiguracaoInvalida(f"{arquivo} deve ter um número inteiro") from None
    arquivo.parent.mkdir(parents=True, exist_ok=True)
    arquivo.write_text(f"{primeiro + quantos}\n")
    return primeiro


def criar_cliente(url: str) -> httpx.Client:
    """Cliente HTTP da saída http; os testes trocam por um TestClient."""
    return httpx.Client(base_url=url, timeout=TIMEOUT_HTTP_S)


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
    try:
        aplicar_ambiente(args, os.environ)
    except ConfiguracaoInvalida as erro:
        log.error("%s", erro)
        return 1

    if not TAXA_TEL_MINIMA_HZ <= args.taxa_tel <= TAXA_TEL_MAXIMA_HZ:
        log.error(
            "--taxa-tel (ou SIMULADOR_TAXA_TEL_HZ) deve ficar entre %d e %d Hz (recebido %g)",
            TAXA_TEL_MINIMA_HZ,
            TAXA_TEL_MAXIMA_HZ,
            args.taxa_tel,
        )
        return 1
    if args.acelerar <= 0:
        log.error(
            "--acelerar (ou SIMULADOR_ACELERAR) deve ser maior que 0 (recebido %g)", args.acelerar
        )
        return 1
    if args.boot is not None and args.boot < 0:
        log.error("--boot deve ser no mínimo 0 (recebido %d)", args.boot)
        return 1
    if args.saida == "http" and not args.url:
        log.error("--saida http precisa de --url ou API_URL")
        return 1
    try:
        roteiro = carregar(args.roteiro)
    except RoteiroInvalido as erro:
        log.error("%s", erro)
        return 1

    try:
        boot = reservar_boots(ARQUIVO_BOOT, len(roteiro.tentativas), args.boot)
    except (ConfiguracaoInvalida, OSError) as erro:
        log.error("boot: %s", erro)
        return 1
    opcoes = Opcoes(boot=boot, taxa_tel_hz=args.taxa_tel)

    interrupcao = Interrupcao()

    def rodar(escrever: Callable[[str], None]) -> int:
        return emitir(
            gerar(roteiro, opcoes),
            escrever,
            acelerar=args.acelerar,
            sem_espera=args.sem_espera,
            interrupcao=interrupcao,
        )

    if args.saida == "pty":
        try:
            saida = SaidaPty(ao_receber=interrupcao.ao_receber)
        except PtyIndisponivel as erro:
            log.error("%s", erro)
            return 1
        with saida:
            log.info("porta serial virtual: %s (esperando alguém abrir)", saida.caminho)
            saida.aguardar_conexao()
            log.info("porta aberta, começando o roteiro")
            enviadas = rodar(saida.escrever)
    elif args.saida == "http":
        with SaidaHttp(criar_cliente(args.url), ao_receber=interrupcao.ao_receber) as saida:
            log.info("enviando para %s/telemetria", args.url.rstrip("/"))
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
