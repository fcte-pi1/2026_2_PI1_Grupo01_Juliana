"""Confere a telemetria que sai na serial da ESP32 (contrato v1).

Lê as linhas da porta serial (ou de um arquivo, para testes), ignora as de
depuração (começam com `#`) e confere, em cada uma das outras, o tamanho, o
JSON, a versão, o tipo e a sequência do `seq` dentro do mesmo `boot`. No fim
imprime um resumo e sai com 0 só se chegou alguma mensagem e não houve buraco
de `seq` nem linha inválida. O que chega antes da primeira mensagem (log de
boot da ESP32, restos repetidos pelo conversor USB-serial) é descartado e só
contado.

Uso:
    python tools/verificar_serial.py --porta /dev/ttyUSB0 --duracao 600
    python tools/verificar_serial.py --arquivo captura.jsonl
"""

import argparse
import json
import sys
import time
from collections import Counter
from collections.abc import Iterable, Iterator
from dataclasses import dataclass, field
from pathlib import Path

TAMANHO_MAXIMO = 256  # bytes, contando o \n
VERSAO = 1
TIPOS = ("hc_item", "hc_resultado", "tel", "passo", "falha", "sucesso")
BAUD_PADRAO = 115200
DURACAO_PADRAO_S = 600.0


def validar(texto: bytes, tamanho: int) -> tuple[dict | None, str | None]:
    """Devolve a mensagem do contrato, ou o motivo de a linha ser inválida."""
    if tamanho > TAMANHO_MAXIMO:
        return None, f"{tamanho} bytes (máximo {TAMANHO_MAXIMO})"
    try:
        msg = json.loads(texto)
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None, "JSON inválido"
    if not isinstance(msg, dict):
        return None, "não é um objeto JSON"
    if msg.get("v") != VERSAO:
        return None, f"v = {msg.get('v')!r}"
    tipo = msg.get("tipo")
    if tipo not in TIPOS:
        return None, f"tipo desconhecido {tipo!r}"
    boot, seq, t_ms = msg.get("boot"), msg.get("seq"), msg.get("t_ms")
    if not all(isinstance(c, int) and not isinstance(c, bool) for c in (boot, seq, t_ms)):
        return None, "boot, seq ou t_ms não é inteiro"
    return msg, None


@dataclass
class Verificacao:
    """Estado acumulado da conferência, linha a linha."""

    por_tipo: Counter = field(default_factory=Counter)
    invalidas: list[tuple[int, str]] = field(default_factory=list)
    buracos: list[tuple[int, int, int]] = field(default_factory=list)  # boot, esperado, recebido
    ultimo_seq: dict[int, int] = field(default_factory=dict)
    t_ms: dict[int, tuple[int, int]] = field(default_factory=dict)  # boot: (primeiro, último)
    tel_por_boot: Counter = field(default_factory=Counter)
    # Ao abrir a porta chegam o log de boot da ESP32 ou restos repetidos pelo
    # conversor USB-serial: o que vem antes da primeira mensagem não conta.
    descartadas_abertura: int = 0

    def conferir(self, numero: int, linha: bytes) -> None:
        """Confere uma linha crua (com ou sem o \\n do fim)."""
        if linha.startswith(b"#"):
            return
        tamanho = len(linha) if linha.endswith(b"\n") else len(linha) + 1
        texto = linha.rstrip(b"\r\n")
        if not texto:
            return
        msg, motivo = validar(texto, tamanho)
        if motivo is not None:
            if self.por_tipo:
                self.invalidas.append((numero, motivo))
            else:
                self.descartadas_abertura += 1
            return

        tipo, boot, seq, t_ms = msg["tipo"], msg["boot"], msg["seq"], msg["t_ms"]
        self.por_tipo[tipo] += 1
        if tipo == "tel":
            self.tel_por_boot[boot] += 1
        anterior = self.ultimo_seq.get(boot)
        if anterior is not None and seq != anterior + 1:
            self.buracos.append((boot, anterior + 1, seq))
        self.ultimo_seq[boot] = seq
        primeiro, _ = self.t_ms.get(boot, (t_ms, t_ms))
        self.t_ms[boot] = (primeiro, t_ms)

    def duracao_s(self) -> float:
        """Soma, por boot, o intervalo entre o primeiro e o último t_ms."""
        return sum(max(ultimo - primeiro, 0) for primeiro, ultimo in self.t_ms.values()) / 1000

    def taxa_tel_hz(self) -> float | None:
        """Taxa média da tel pelo t_ms; None se não deu para medir."""
        duracao = self.duracao_s()
        if duracao <= 0 or not self.tel_por_boot:
            return None
        return sum(self.tel_por_boot.values()) / duracao

    def ok(self) -> bool:
        """Verdadeiro se chegou alguma mensagem e não houve buraco de seq nem linha inválida."""
        return bool(self.por_tipo) and not self.buracos and not self.invalidas

    def resumo(self) -> str:
        """Texto do resumo impresso no fim."""
        linhas = ["Linhas por tipo:"]
        for tipo in TIPOS:
            linhas.append(f"  {tipo}: {self.por_tipo[tipo]}")
        taxa = self.taxa_tel_hz()
        texto_taxa = f"{taxa:.2f} Hz" if taxa is not None else "sem dados"
        linhas.append(f"Taxa média da tel: {texto_taxa}")
        linhas.append(f"Duração (pelo t_ms): {self.duracao_s():.1f} s")
        linhas.append(f"Buracos de seq: {len(self.buracos)}")
        for boot, esperado, recebido in self.buracos:
            linhas.append(f"  boot {boot}: esperado seq {esperado}, recebido seq {recebido}")
        linhas.append(f"Descartadas antes da primeira mensagem: {self.descartadas_abertura}")
        linhas.append(f"Linhas inválidas: {len(self.invalidas)}")
        for numero, motivo in self.invalidas:
            linhas.append(f"  linha {numero}: {motivo}")
        if not self.por_tipo:
            linhas.append("Nenhuma mensagem do contrato recebida")
        linhas.append("Resultado: OK" if self.ok() else "Resultado: FALHOU")
        return "\n".join(linhas)


def verificar(linhas: Iterable[bytes]) -> Verificacao:
    """Confere todas as linhas e devolve o estado acumulado."""
    verificacao = Verificacao()
    for numero, linha in enumerate(linhas, start=1):
        verificacao.conferir(numero, linha)
    return verificacao


def ler_serial(porta: str, baud: int, duracao_s: float) -> Iterator[bytes]:
    """Lê linhas da porta serial até acabar a duração."""
    import serial  # só no modo porta: o modo --arquivo não precisa do pyserial

    conexao = serial.Serial()
    conexao.port = porta
    conexao.baudrate = baud
    conexao.timeout = 0.5
    # Sem mexer em DTR e RTS a ESP32 não reinicia ao abrir a porta.
    conexao.dtr = False
    conexao.rts = False
    conexao.open()
    with conexao:
        yield from ler_linhas(conexao, duracao_s)


def ler_linhas(conexao, duracao_s: float) -> Iterator[bytes]:
    """Lê linhas completas de uma conexão já aberta até acabar a duração."""
    fim = time.monotonic() + duracao_s
    # A porta pode abrir no meio de uma linha: limpa a entrada e descarta
    # tudo até o primeiro fim de linha, para não contar o pedaço como inválido.
    conexao.reset_input_buffer()
    descartar = True
    # O readline devolve só o que chegou até o timeout, então os pedaços
    # ficam acumulados até completar a linha.
    linha = b""
    while time.monotonic() < fim:
        linha += conexao.readline()
        if not linha.endswith(b"\n"):
            continue
        if not descartar:
            yield linha
        descartar = False
        linha = b""


def criar_parser() -> argparse.ArgumentParser:
    """Monta o parser com as opções do verificador."""
    parser = argparse.ArgumentParser(
        description="Confere a telemetria do contrato v1 que sai na serial da ESP32.",
    )
    origem = parser.add_mutually_exclusive_group(required=True)
    origem.add_argument("--porta", help="porta serial, por exemplo /dev/ttyUSB0")
    origem.add_argument("--arquivo", type=Path, help="arquivo com as linhas já capturadas")
    parser.add_argument(
        "--baud", type=int, default=BAUD_PADRAO, help=f"padrão: {BAUD_PADRAO}"
    )
    parser.add_argument(
        "--duracao",
        type=float,
        default=DURACAO_PADRAO_S,
        metavar="S",
        help=f"segundos de leitura da porta (padrão: {DURACAO_PADRAO_S:.0f})",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Roda a verificação e devolve o código de saída."""
    args = criar_parser().parse_args(argv)
    if args.arquivo is not None:
        with args.arquivo.open("rb") as arquivo:
            verificacao = verificar(arquivo)
    else:
        verificacao = verificar(ler_serial(args.porta, args.baud, args.duracao))
    print(verificacao.resumo())
    return 0 if verificacao.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
