"""Confere a telemetria que sai na serial da ESP32 (contrato v1).

Lê as linhas da porta serial (ou de um arquivo, para testes), ignora as de
depuração (começam com `#`) e confere, em cada uma das outras, o tamanho, o
JSON, a versão, o tipo e a sequência do `seq` dentro do mesmo `boot`. No fim
imprime um resumo e sai com 0 só se não houver buraco de `seq` nem linha
inválida.

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


@dataclass
class Verificacao:
    """Estado acumulado da conferência, linha a linha."""

    por_tipo: Counter = field(default_factory=Counter)
    invalidas: list[tuple[int, str]] = field(default_factory=list)
    buracos: list[tuple[int, int, int]] = field(default_factory=list)  # boot, esperado, recebido
    ultimo_seq: dict[int, int] = field(default_factory=dict)
    t_ms: dict[int, tuple[int, int]] = field(default_factory=dict)  # boot: (primeiro, último)
    tel_por_boot: Counter = field(default_factory=Counter)

    def conferir(self, numero: int, linha: bytes) -> None:
        """Confere uma linha crua (com ou sem o \\n do fim)."""
        if linha.startswith(b"#"):
            return
        tamanho = len(linha) if linha.endswith(b"\n") else len(linha) + 1
        texto = linha.rstrip(b"\r\n")
        if not texto:
            return
        if tamanho > TAMANHO_MAXIMO:
            self.invalidas.append((numero, f"{tamanho} bytes (máximo {TAMANHO_MAXIMO})"))
            return
        try:
            msg = json.loads(texto)
        except (UnicodeDecodeError, json.JSONDecodeError):
            self.invalidas.append((numero, "JSON inválido"))
            return
        if not isinstance(msg, dict):
            self.invalidas.append((numero, "não é um objeto JSON"))
            return
        if msg.get("v") != VERSAO:
            self.invalidas.append((numero, f"v = {msg.get('v')!r}"))
            return
        tipo = msg.get("tipo")
        if tipo not in TIPOS:
            self.invalidas.append((numero, f"tipo desconhecido {tipo!r}"))
            return
        boot, seq, t_ms = msg.get("boot"), msg.get("seq"), msg.get("t_ms")
        if not all(isinstance(c, int) and not isinstance(c, bool) for c in (boot, seq, t_ms)):
            self.invalidas.append((numero, "boot, seq ou t_ms não é inteiro"))
            return

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
        """Verdadeiro se não houve buraco de seq nem linha inválida."""
        return not self.buracos and not self.invalidas

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
        linhas.append(f"Linhas inválidas: {len(self.invalidas)}")
        for numero, motivo in self.invalidas:
            linhas.append(f"  linha {numero}: {motivo}")
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
    fim = time.monotonic() + duracao_s
    with conexao:
        while time.monotonic() < fim:
            linha = conexao.readline()
            if linha.endswith(b"\n"):
                yield linha


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
