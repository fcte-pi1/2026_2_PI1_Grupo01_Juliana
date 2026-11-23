"""Confere a telemetria que sai na serial da ESP32 (contrato v1).

Lê as linhas da porta serial (ou de um arquivo, para testes), ignora as de
depuração (começam com `#`) e confere, em cada uma das outras, o tamanho, o
JSON, a versão, o tipo e a sequência do `seq` dentro do mesmo `boot`. No fim
imprime um resumo e sai com 0 só se chegou alguma mensagem e não houve buraco
de `seq` nem linha inválida. O que chega antes da primeira mensagem (log de
boot da ESP32, restos repetidos pelo conversor USB-serial) é descartado e só
contado.

Com a máquina de estados o robô espera o comando `largar` depois do health-check;
`--largar` manda o comando uma vez, alguns segundos depois de abrir a porta, para o
teste cobrir também os `passo` e o `sucesso`.

Uso:
    python tools/verificar_serial.py --porta /dev/ttyUSB0 --duracao 600
    python tools/verificar_serial.py --porta /dev/ttyUSB0 --duracao 600 --largar
    python tools/verificar_serial.py --arquivo captura.jsonl
    python tools/verificar_serial.py --porta /dev/rfcomm0 --duracao 600 --largar
    python tools/verificar_serial.py --porta /dev/rfcomm0 --duracao 60 --health-check

Pela porta, o resumo traz o atraso de entrega das mensagens (variação de chegada menos `t_ms`) e o
resultado falha se o p95 passar de 3 s, o critério da FIRM-11 para o Bluetooth.
"""

import argparse
import json
import sys
import threading
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
LARGAR_APOS_PADRAO_S = 5.0  # o health-check do boot leva cerca de 1,5 s
COMANDO_LARGAR = b'{"v":1,"cmd":"largar"}\n'
COMANDO_HEALTH_CHECK = b'{"v":1,"cmd":"health_check"}\n'
ATRASO_P95_MAXIMO_S = 3.0  # critério da FIRM-11 para a entrega pelo Bluetooth


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
    # (boot, chegada em s, t_ms do robô): só no modo porta, onde a hora de chegada é conhecida.
    chegadas: list[tuple[int, float, int, str]] = field(default_factory=list)  # boot, chegada, t_ms, tipo
    sequencia: list[tuple[int, int, int, str]] = field(default_factory=list)  # boot, seq, t_ms, tipo (modo porta)
    atraso_p95_maximo_s: float = ATRASO_P95_MAXIMO_S
    quedas: int = 0  # vezes em que o enlace caiu depois de entregar linhas (modo porta)
    sem_enlace: int = 0  # tentativas de abrir a conexão que não entregaram nenhuma linha
    ultima_chegada_s: float | None = None

    def conferir(self, numero: int, linha: bytes, chegada_s: float | None = None) -> None:
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
        if chegada_s is not None:
            self.chegadas.append((boot, chegada_s, t_ms, tipo))
            self.ultima_chegada_s = chegada_s
        if tipo == "tel":
            self.tel_por_boot[boot] += 1
        if chegada_s is not None:
            # No modo porta os buracos são calculados no fim, sem as mensagens guardadas desde o boot.
            self.sequencia.append((boot, seq, t_ms, tipo))
        else:
            anterior = self.ultimo_seq.get(boot)
            if anterior is not None and seq != anterior + 1:
                self.buracos.append((boot, anterior + 1, seq))
            self.ultimo_seq[boot] = seq
        primeiro, _ = self.t_ms.get(boot, (t_ms, t_ms))
        self.t_ms[boot] = (primeiro, t_ms)

    def _ao_vivo(self) -> list[tuple[int, int, str]]:
        """(boot, t_ms, tipo) das mensagens ao vivo, sem as reenviadas da fila de eventos."""
        ao_vivo = []
        for boot in {b for b, _, _, _ in self.chegadas}:
            fora = self._guardadas_antes_da_conexao(boot, self.chegadas)
            ao_vivo += [
                (b, t_ms, tipo)
                for i, (b, _, t_ms, tipo) in enumerate(self.chegadas)
                if b == boot and i not in fora
            ]
        return ao_vivo

    def duracao_s(self) -> float:
        """Intervalo entre a primeira e a última mensagem, por boot, pelo t_ms.

        No modo porta só conta o que chegou ao vivo: as reenviadas do boot esticariam o intervalo.
        """
        if self.chegadas:
            ao_vivo = self._ao_vivo()
            total = 0
            for boot in {b for b, _, _ in ao_vivo}:
                tempos = [t for b, t, _ in ao_vivo if b == boot]
                total += max(tempos) - min(tempos)
            return total / 1000
        return sum(max(ultimo - primeiro, 0) for primeiro, ultimo in self.t_ms.values()) / 1000

    def taxa_tel_hz(self) -> float | None:
        """Taxa média da tel pelo t_ms; None se não deu para medir."""
        duracao = self.duracao_s()
        if self.chegadas:
            tel = sum(1 for _, _, tipo in self._ao_vivo() if tipo == "tel")
        else:
            tel = sum(self.tel_por_boot.values())
        if duracao <= 0 or not tel:
            return None
        return tel / duracao

    def _guardadas_antes_da_conexao(self, boot: int, chegadas: list) -> set[int]:
        """Índices das mensagens geradas antes da primeira `tel` recebida do boot.

        O robô guarda os eventos (hc_item, passo, falha) enquanto ninguém está conectado e os entrega
        quando a conexão abre: o tempo que esperaram não é atraso do enlace.
        """
        referencia = next((t_ms for b, _, t_ms, tipo in chegadas if b == boot and tipo == "tel"), None)
        if referencia is None:
            return set()
        return {
            i for i, (b, _, t_ms, _) in enumerate(chegadas) if b == boot and t_ms < referencia
        }

    def guardadas_antes_da_conexao(self) -> int:
        """Quantas mensagens chegaram depois de esperar a conexão abrir."""
        total = 0
        for boot in {boot for boot, _, _, _ in self.chegadas}:
            total += len(self._guardadas_antes_da_conexao(boot, self.chegadas))
        return total

    def maior_silencio_s(self) -> float | None:
        """Maior intervalo entre duas mensagens que chegaram (pela hora do notebook)."""
        horas = sorted(chegada for _, chegada, _, _ in self.chegadas)
        if len(horas) < 2:
            return None
        return max(depois - antes for antes, depois in zip(horas, horas[1:], strict=False))

    def _recalcular_buracos(self) -> None:
        """Buracos de seq só entre as mensagens ao vivo.

        O robô guarda os eventos (hc_item, passo, falha) enquanto ninguém está conectado e os reenvia
        com o seq original a cada conexão nova, e a `tel` segue o seq geral do boot: entrar no meio de
        um boot mostra um salto que não é perda. Perda é seq faltando depois da primeira `tel` recebida.
        """
        if not self.sequencia:
            return
        buracos: list[tuple[int, int, int]] = []
        for boot in {b for b, _, _, _ in self.sequencia}:
            referencia = next((t for b, _, t, tipo in self.sequencia if b == boot and tipo == "tel"), None)
            anterior = None
            for b, seq, t_ms, tipo in self.sequencia:
                if b != boot:
                    continue
                if referencia is not None and tipo != "tel" and t_ms < referencia:
                    continue  # reenviada da fila de eventos
                if anterior is not None and seq != anterior + 1:
                    buracos.append((boot, anterior + 1, seq))
                anterior = seq
        self.buracos = buracos

    def atrasos_s(self) -> list[float]:
        """Atraso de entrega de cada mensagem: a variação de (chegada - t_ms) dentro do boot.

        O relógio do robô e o do notebook não estão alinhados, então vale só a diferença para a
        mensagem que chegou mais rápida do mesmo boot (o menor atraso conta como zero). Ficam de
        fora as mensagens guardadas desde antes de a conexão abrir.
        """
        atrasos: list[float] = []
        for boot in {boot for boot, _, _, _ in self.chegadas}:
            fora = self._guardadas_antes_da_conexao(boot, self.chegadas)
            desvios = [
                chegada - t_ms / 1000
                for i, (b, chegada, t_ms, _) in enumerate(self.chegadas)
                if b == boot and i not in fora
            ]
            if not desvios:
                continue
            base = min(desvios)
            atrasos.extend(desvio - base for desvio in desvios)
        return atrasos

    def atraso_p95_s(self) -> float | None:
        """Percentil 95 do atraso de entrega; None sem hora de chegada (modo arquivo)."""
        atrasos = sorted(self.atrasos_s())
        if not atrasos:
            return None
        return atrasos[min(len(atrasos) - 1, int(0.95 * len(atrasos)))]

    def atraso_maximo_s(self) -> float | None:
        atrasos = self.atrasos_s()
        return max(atrasos) if atrasos else None

    def atraso_ok(self) -> bool:
        p95 = self.atraso_p95_s()
        return p95 is None or p95 <= self.atraso_p95_maximo_s

    def ok(self) -> bool:
        """Verdadeiro se chegou alguma mensagem, sem buraco de seq, linha inválida nem atraso de entrega alto."""
        self._recalcular_buracos()
        return (
            bool(self.por_tipo)
            and not self.buracos
            and not self.invalidas
            and self.atraso_ok()
            and self.quedas == 0
        )

    def resumo(self) -> str:
        """Texto do resumo impresso no fim."""
        linhas = ["Linhas por tipo:"]
        for tipo in TIPOS:
            linhas.append(f"  {tipo}: {self.por_tipo[tipo]}")
        taxa = self.taxa_tel_hz()
        texto_taxa = f"{taxa:.2f} Hz" if taxa is not None else "sem dados"
        linhas.append(f"Taxa média da tel: {texto_taxa}")
        linhas.append(f"Duração (pelo t_ms): {self.duracao_s():.1f} s")
        p95 = self.atraso_p95_s()
        if p95 is not None:
            linhas.append(
                f"Atraso de entrega: p95 {p95:.2f} s, máximo {self.atraso_maximo_s():.2f} s "
                f"(limite do p95: {self.atraso_p95_maximo_s:.1f} s)"
            )
        silencio = self.maior_silencio_s()
        if silencio is not None:
            linhas.append(f"Maior silêncio entre mensagens: {silencio:.1f} s")
        guardadas = self.guardadas_antes_da_conexao()
        if guardadas:
            linhas.append(
                f"Mensagens guardadas pelo robô antes de a conexão abrir (fora do atraso): {guardadas}"
            )
        linhas.append(f"Quedas do enlace: {self.quedas}")
        if self.sem_enlace:
            linhas.append(f"Tentativas de conexão sem nenhuma mensagem: {self.sem_enlace}")
        self._recalcular_buracos()
        linhas.append(f"Buracos de seq: {len(self.buracos)}")
        for boot, esperado, recebido in self.buracos:
            linhas.append(f"  boot {boot}: esperado seq {esperado}, recebido seq {recebido}")
        linhas.append(f"Descartadas antes da primeira mensagem: {self.descartadas_abertura}")
        linhas.append(f"Linhas inválidas: {len(self.invalidas)}")
        for numero, motivo in self.invalidas:
            linhas.append(f"  linha {numero}: {motivo}")
        if not self.por_tipo:
            linhas.append("Nenhuma mensagem do contrato recebida")
            if self.sem_enlace:
                linhas.append(
                    "O enlace não abriu: confira o pareamento, se há outro programa na porta, "
                    "e reinicie a placa (desligue e ligue o interruptor)"
                )
        linhas.append("Resultado: OK" if self.ok() else "Resultado: FALHOU")
        return "\n".join(linhas)


def verificar(
    linhas: Iterable[bytes | None], relogio=None, verificacao: Verificacao | None = None
) -> Verificacao:
    """Confere todas as linhas e devolve o estado acumulado.

    Com `relogio` (uma função que devolve segundos), anota a hora em que cada linha foi lida,
    o que permite medir o atraso de entrega; vale para quem lê a porta ao vivo.
    """
    verificacao = verificacao if verificacao is not None else Verificacao()
    for numero, linha in enumerate(linhas, start=1):
        if linha is SEM_ENLACE:  # a conexão não abriu
            verificacao.sem_enlace += 1
            continue
        if linha is None:  # o enlace caiu e foi reaberto
            verificacao.quedas += 1
            continue
        verificacao.conferir(numero, linha, relogio() if relogio is not None else None)
    return verificacao


SEM_ENLACE = object()  # a conexão não chegou a entregar nenhuma linha
ESPERAS_REABRIR_S = (0.5, 1.0, 2.0)  # depois da última, repete 2 s


def _eh_bluetooth(porta: str) -> bool:
    """Porta criada pelo `rfcomm bind` (Linux); no Windows o SPP aparece como COMx e não é distinguível."""
    return "rfcomm" in porta


def _abrir_serial(porta: str, baud: int):
    """Abre a porta sem mexer em DTR e RTS (a ESP32 não reinicia ao abrir)."""
    import serial  # só no modo porta: o modo --arquivo não precisa do pyserial

    conexao = serial.Serial()
    conexao.port = porta
    conexao.baudrate = baud
    conexao.timeout = 0.5
    if not _eh_bluetooth(porta):
        # No USB, DTR e RTS em baixa evitam o reinício da ESP32; no rfcomm o DTR baixo derruba a conexão.
        conexao.dtr = False
        conexao.rts = False
    conexao.open()
    return conexao


def ler_serial(
    porta: str,
    baud: int,
    duracao_s: float,
    largar_apos_s: float | None = None,
    abrir=_abrir_serial,
    dormir=time.sleep,
    comando: bytes = COMANDO_LARGAR,
) -> Iterator[bytes | None]:
    """Lê linhas da porta serial até acabar a duração.

    Se o enlace cair depois de ter entregue linhas, entrega `None` (uma queda); se cair sem nunca
    ter entregado nada, entrega `SEM_ENLACE` (a conexão não abriu). Nos dois casos reabre a porta
    com esperas crescentes até a duração acabar.
    """
    fim = time.monotonic() + duracao_s
    primeira = True
    espera = 0
    while time.monotonic() < fim:
        try:
            conexao = abrir(porta, baud)
        except OSError:
            dormir(ESPERAS_REABRIR_S[min(espera, len(ESPERAS_REABRIR_S) - 1)])
            espera += 1
            continue
        espera = 0
        entregou = False
        try:
            with conexao:
                for linha in ler_linhas(
                    conexao,
                    fim - time.monotonic(),
                    largar_apos_s if primeira else None,
                    comando,
                    limpar_inicio=not _eh_bluetooth(porta),
                ):
                    entregou = True
                    yield linha
            return
        except OSError:
            primeira = False
            yield None if entregou else SEM_ENLACE
            dormir(ESPERAS_REABRIR_S[0])


def ler_linhas(
    conexao,
    duracao_s: float,
    largar_apos_s: float | None = None,
    comando: bytes = COMANDO_LARGAR,
    limpar_inicio: bool = True,
) -> Iterator[bytes]:
    """Lê linhas completas de uma conexão já aberta até acabar a duração.

    Com `largar_apos_s`, escreve o comando `largar` uma vez quando esse tempo passar.
    """
    inicio = time.monotonic()
    fim = inicio + duracao_s
    largado = largar_apos_s is None
    # A porta pode abrir no meio de uma linha: limpa a entrada e descarta
    # tudo até o primeiro fim de linha, para não contar o pedaço como inválido.
    # No Bluetooth a conexão nasce no começo de uma mensagem: limpar a entrada e descartar a primeira linha
    # jogaria fora mensagens boas (o robô reenvia os eventos guardados logo ao conectar).
    if limpar_inicio:
        conexao.reset_input_buffer()
    descartar = limpar_inicio
    # O readline devolve só o que chegou até o timeout, então os pedaços
    # ficam acumulados até completar a linha.
    linha = b""
    while time.monotonic() < fim:
        if not largado and time.monotonic() - inicio >= largar_apos_s:
            conexao.write(comando)
            largado = True
        linha += conexao.readline()
        if not linha.endswith(b"\n"):
            continue
        if not descartar:
            yield linha
        descartar = False
        linha = b""


ANDAMENTO_A_CADA_S = 10.0


def _andamento(verificacao: Verificacao, inicio: float, parar: threading.Event) -> None:
    """Mostra no stderr, a cada 10 s, o que já chegou, para a leitura longa não parecer travada."""
    while not parar.wait(ANDAMENTO_A_CADA_S):
        agora = time.monotonic()
        ultima = verificacao.ultima_chegada_s
        sem = f"{agora - ultima:.0f} s" if ultima is not None else "nenhuma ainda"
        total = sum(verificacao.por_tipo.values())
        print(
            f"[{agora - inicio:4.0f} s] mensagens: {total} (tel {verificacao.por_tipo['tel']}), "
            f"quedas do enlace: {verificacao.quedas}, conexões sem mensagem: {verificacao.sem_enlace}, "
            f"última mensagem há {sem}",
            file=sys.stderr,
            flush=True,
        )


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
    parser.add_argument(
        "--largar",
        action="store_true",
        help="manda o comando largar uma vez depois do health-check (só com --porta)",
    )
    parser.add_argument(
        "--health-check",
        action="store_true",
        help="manda o comando health_check uma vez, 5 s depois de abrir a porta, para aparecerem "
        "os hc_item e o hc_resultado (só com --porta; o autoteste gira os motores: rodas no ar)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    """Roda a verificação e devolve o código de saída."""
    args = criar_parser().parse_args(argv)
    if args.arquivo is not None:
        with args.arquivo.open("rb") as arquivo:
            verificacao = verificar(arquivo)
    else:
        largar_apos = LARGAR_APOS_PADRAO_S if (args.largar or args.health_check) else None
        comando = COMANDO_HEALTH_CHECK if args.health_check else COMANDO_LARGAR
        verificacao = Verificacao()
        print(
            f"Lendo {args.porta} por {args.duracao:.0f} s. O resumo sai no fim; Ctrl+C encerra antes "
            "e mostra o que foi lido.",
            file=sys.stderr,
        )
        parar = threading.Event()
        inicio = time.monotonic()
        threading.Thread(
            target=_andamento, args=(verificacao, inicio, parar), daemon=True
        ).start()
        try:
            verificar(
                ler_serial(args.porta, args.baud, args.duracao, largar_apos, comando=comando),
                relogio=time.monotonic,
                verificacao=verificacao,
            )
        except KeyboardInterrupt:
            print("\nInterrompido pelo teclado.", file=sys.stderr)
        finally:
            parar.set()
    print(verificacao.resumo())
    return 0 if verificacao.ok() else 1


if __name__ == "__main__":
    sys.exit(main())
