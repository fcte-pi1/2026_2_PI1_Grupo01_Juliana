"""Gerador: transforma o roteiro numa linha do tempo de mensagens do contrato.

Função pura, sem relógio nem I/O: cada item sai com o instante simulado em que
deve ser enviado. Quem dorme, acelera e escreve é o emissor.
"""

from collections import deque
from collections.abc import Iterator
from dataclasses import dataclass
from typing import get_args

from contrato.telemetria import (
    VERSAO,
    Componente,
    Falha,
    HcItem,
    HcResultado,
    Mensagem,
    Passo,
    Sucesso,
    Tel,
)

from simulador.roteiro import Celula, FimFalha, FimSucesso, ParadaBoot, Roteiro, Tentativa

COMPONENTES: tuple[Componente, ...] = get_args(Componente)  # ordem da Tabela 3

PERIODO_TEL_PARADO_MS = 1000  # tel a 1 Hz parado
INTERVALO_HC_ITEM_MS = 200  # entre um teste de componente e o próximo
DURACAO_HC_MS = 2000  # hc_item em 100, 300, ..., 1500 ms; hc_resultado em 1900 ms
DURACAO_FINAL_MS = 3000  # tel a 1 Hz em success/failed depois do fim
TAMANHO_CELULA_MM = 180
VELOCIDADE_MAXIMA_MM_S = 2000  # teto do vel_mm_s no contrato
TAMANHO_BUFFER = 64  # eventos guardados para reenvio (buffer circular do firmware)
INTERVALO_REENVIO_MS = 1000 // 20  # reenvio a no máximo 20 por segundo


@dataclass(frozen=True)
class Opcoes:
    """Parâmetros da geração que não vêm do roteiro."""

    boot: int = 0
    taxa_tel_hz: float = 5


@dataclass(frozen=True)
class LinhaCrua:
    """Texto enviado como está, mesmo que não seja uma mensagem válida."""

    linha: str


Item = Mensagem | LinhaCrua


class _Boot:
    """Estado de um boot: numera as mensagens e conta o tempo desde o início."""

    def __init__(
        self, roteiro: Roteiro, tentativa: Tentativa, opcoes: Opcoes, boot: int, inicio_ms: int
    ):
        self.roteiro = roteiro
        self.opcoes = opcoes
        self.tentativa = tentativa
        self.boot = boot
        self.inicio_ms = inicio_ms
        self.seq = 0
        celula = tentativa.celulas[0]
        self.x, self.y = celula.x, celula.y
        self.rumo = "N"
        self.eixo_longo = _eixo_longo(roteiro.labirinto, self.x, self.y, None)

    def mensagem(self, modelo: type, t_ms: int, **campos) -> tuple[int, Mensagem]:
        """Monta a próxima mensagem do boot e devolve (t simulado, mensagem)."""
        mensagem = modelo(v=VERSAO, boot=self.boot, seq=self.seq, t_ms=t_ms, **campos)
        self.seq += 1
        return self.inicio_ms + t_ms, mensagem

    def tel(self, t_ms: int, estado: str, vel_mm_s: int = 0) -> tuple[int, Mensagem]:
        bat_mv = max(0, self.roteiro.bateria_inicial_mv - t_ms // 1000)  # 1 mV/s
        return self.mensagem(
            Tel,
            t_ms,
            tipo="tel",
            estado=estado,
            x=self.x,
            y=self.y,
            rumo=self.rumo,
            bat_mv=bat_mv,
            vel_mm_s=vel_mm_s,
            eixo_longo=self.eixo_longo,
        )

    def entrar(self, celula: Celula) -> None:
        """Move o robô para a célula e atualiza rumo e eixo longo."""
        rumo = _rumo(celula.x - self.x, celula.y - self.y)
        if rumo is not None:
            self.rumo = rumo
        self.x, self.y = celula.x, celula.y
        self.eixo_longo = _eixo_longo(self.roteiro.labirinto, self.x, self.y, self.eixo_longo)


def _rumo(dx: int, dy: int) -> str | None:
    """Direção absoluta do passo: N = +y, S = -y, L = +x, O = -x."""
    return {(0, 1): "N", (0, -1): "S", (1, 0): "L", (-1, 0): "O"}.get((dx, dy))


def _eixo_longo(labirinto: str, x: int, y: int, atual: str | None) -> str | None:
    """O eixo longo fica conhecido na primeira célula além da coluna/linha 3."""
    if atual is not None or labirinto == "4x4":
        return atual
    if x > 3:
        return "x"
    if y > 3:
        return "y"
    return None


def _health_check(estado: _Boot, inicio: str) -> Iterator[tuple[int, Mensagem]]:
    """8 hc_item e o hc_resultado, com tel a 1 Hz em estado health-check."""
    reprovados = set(estado.tentativa.hc_reprovados)
    labirinto = estado.roteiro.labirinto
    aprovado = not reprovados and labirinto != "invalido"
    agenda = [
        (t, 0, "tel", {}) for t in range(0, DURACAO_HC_MS, PERIODO_TEL_PARADO_MS)
    ]  # na mesma hora, a tel sai antes do evento
    for i, componente in enumerate(COMPONENTES):
        campos = {"componente": componente, "aprovado": componente not in reprovados}
        t = INTERVALO_HC_ITEM_MS // 2 + i * INTERVALO_HC_ITEM_MS
        agenda.append((t, 1, "hc_item", campos | {"valor": None}))
    campos = {"aprovado": aprovado, "tipo_dip": labirinto, "inicio": inicio}
    agenda.append((DURACAO_HC_MS - INTERVALO_HC_ITEM_MS // 2, 1, "hc_resultado", campos))

    modelos = {"hc_item": HcItem, "hc_resultado": HcResultado}
    for t, _, tipo, campos in sorted(agenda, key=lambda item: item[:2]):
        if tipo == "tel":
            yield estado.tel(t, "health-check")
        else:
            yield estado.mensagem(modelos[tipo], t, tipo=tipo, **campos)
    return aprovado


def _parada(tentativa: Tentativa) -> ParadaBoot | None:
    """Primeira parada pelo BOOT da tentativa, se houver."""
    paradas = [evento for evento in tentativa.eventos if evento.tipo == "parada_boot"]
    return min(paradas, key=lambda evento: evento.na_celula, default=None)


def _ultima_celula(tentativa: Tentativa) -> int:
    """Índice da célula em que a corrida acaba: a da parada pelo BOOT ou a última."""
    parada = _parada(tentativa)
    return len(tentativa.celulas) - 1 if parada is None else parada.na_celula


def _corrida(estado: _Boot, inicio_ms: int) -> Iterator[tuple[int, Item]]:
    """Um passo por célula, tel na taxa de movimento, fim e tel a 1 Hz depois dele.

    A parada pelo BOOT corta a corrida na sua célula e vira o fim. Devolve (pelo return)
    o t_ms em que a tentativa termina.
    """
    tentativa = estado.tentativa
    ultima = _ultima_celula(tentativa)
    celulas = tentativa.celulas[: ultima + 1]
    s_por_celula = estado.roteiro.s_por_celula
    periodo_celula_ms = round(s_por_celula * 1000)
    fim_ms = inicio_ms + ultima * periodo_celula_ms
    vel_mm_s = min(VELOCIDADE_MAXIMA_MM_S, round(TAMANHO_CELULA_MM / s_por_celula))

    # Prioridades no mesmo instante: entrar na célula, tel, passo, linha crua, fim.
    agenda: list[tuple[int, int, str, object]] = []
    for i, celula in enumerate(celulas):
        t = inicio_ms + i * periodo_celula_ms
        agenda += [(t, 0, "entrar", celula), (t, 2, "passo", celula)]
    periodo_tel_ms = round(1000 / estado.opcoes.taxa_tel_hz)
    agenda += [(t, 1, "tel", "running") for t in range(inicio_ms, fim_ms + 1, periodo_tel_ms)]
    for evento in tentativa.eventos:
        if evento.tipo == "linha_crua" and evento.na_celula <= ultima:
            t = inicio_ms + evento.na_celula * periodo_celula_ms
            agenda.append((t, 3, "linha_crua", evento.linha))

    fim = _parada(tentativa) or tentativa.fim
    if isinstance(fim, FimSucesso | FimFalha | ParadaBoot):
        estado_final = "success" if isinstance(fim, FimSucesso) else "failed"
        agenda.append((fim_ms, 4, "fim", fim))
        agenda += [
            (t, 1, "tel", estado_final)
            for t in range(
                fim_ms + PERIODO_TEL_PARADO_MS, fim_ms + DURACAO_FINAL_MS + 1, PERIODO_TEL_PARADO_MS
            )
        ]
        fim_ms += DURACAO_FINAL_MS

    for t, _, acao, dado in sorted(agenda, key=lambda item: item[:2]):
        if acao == "entrar":
            estado.entrar(dado)
        elif acao == "tel":
            yield estado.tel(t, dado, vel_mm_s if dado == "running" else 0)
        elif acao == "passo":
            yield estado.mensagem(Passo, t, tipo="passo", x=dado.x, y=dado.y, paredes=dado.paredes)
        elif acao == "linha_crua":
            yield estado.inicio_ms + t, LinhaCrua(dado)
        elif isinstance(dado, FimSucesso):
            yield estado.mensagem(Sucesso, t, tipo="sucesso", x=estado.x, y=estado.y)
        elif isinstance(dado, ParadaBoot):
            yield estado.mensagem(
                Falha,
                t,
                tipo="falha",
                motivo="encerrado_operador",
                origem="boot",
                x=estado.x,
                y=estado.y,
                componente=None,
            )
        else:
            yield estado.mensagem(
                Falha,
                t,
                tipo="falha",
                motivo=dado.motivo,
                origem="automatica",
                x=estado.x,
                y=estado.y,
                componente=dado.componente,
            )
    return fim_ms


def _tentativa(estado: _Boot, inicio: str) -> Iterator[tuple[int, Item]]:
    """Health-check e, se aprovado, a corrida. Devolve (pelo return) o t_ms final."""
    aprovado = yield from _health_check(estado, inicio)
    if not aprovado:
        return DURACAO_HC_MS  # health-check reprovado: a tentativa termina aqui
    return (yield from _corrida(estado, DURACAO_HC_MS))


def _quedas(estado: _Boot) -> list[tuple[int, int]]:
    """Intervalos [início, fim) de t simulado sem link, já unidos quando se sobrepõem."""
    periodo_celula_ms = round(estado.roteiro.s_por_celula * 1000)
    quedas = sorted(
        (t, t + round(evento.segundos * 1000))
        for evento in estado.tentativa.eventos
        if evento.tipo == "perda_link" and evento.na_celula <= _ultima_celula(estado.tentativa)
        for t in [estado.inicio_ms + DURACAO_HC_MS + evento.na_celula * periodo_celula_ms]
    )
    unidas: list[tuple[int, int]] = []
    for inicio, fim in quedas:
        if unidas and inicio <= unidas[-1][1]:
            unidas[-1] = (unidas[-1][0], max(unidas[-1][1], fim))
        else:
            unidas.append((inicio, fim))
    return unidas


def _eh_evento(item: Item) -> bool:
    return not isinstance(item, Tel | LinhaCrua)


def _com_link(estado: _Boot, itens: Iterator[tuple[int, Item]]) -> Iterator[tuple[int, Item]]:
    """Aplica as quedas de link de um boot à linha do tempo `itens` (de _tentativa).

    Durante a queda nada sai, mas todo evento entra no buffer circular. Na volta, o buffer
    inteiro é reenviado do mais antigo ao mais novo, um a cada INTERVALO_REENVIO_MS; os
    eventos novos entram no fim da fila e a tel continua saindo na hora (seção Reconexão
    do contrato). Devolve (pelo return) o t simulado em que o boot termina.
    """
    buffer: deque[Item] = deque(maxlen=TAMANHO_BUFFER)
    fila: deque[Item] = deque()
    proxima_ms = 0
    pendentes = deque(_quedas(estado))
    fim_ms = 0

    def drenar(ate_ms: int) -> Iterator[tuple[int, Item]]:
        """Envia a fila até `ate_ms`, parando nas quedas e recomeçando em cada volta."""
        nonlocal fila, proxima_ms
        while True:
            caiu = bool(pendentes) and pendentes[0][0] <= ate_ms
            limite_ms = pendentes[0][0] if caiu else ate_ms + 1
            while fila and proxima_ms < limite_ms:
                yield proxima_ms, fila.popleft()
                proxima_ms += INTERVALO_REENVIO_MS
            if not caiu or pendentes[0][1] > ate_ms:
                return
            _, volta_ms = pendentes.popleft()
            fila, proxima_ms = deque(buffer), volta_ms

    def tentativa() -> Iterator[tuple[int, Item]]:
        nonlocal fim_ms
        fim_ms = yield from itens

    t = estado.inicio_ms
    for t, item in tentativa():
        yield from drenar(t)
        sem_link = bool(pendentes) and pendentes[0][0] <= t
        if _eh_evento(item):
            buffer.append(item)
            if sem_link:
                continue
            if fila:
                fila.append(item)  # sai depois do que ainda falta reenviar
                continue
        if not sem_link:
            yield t, item

    if pendentes and pendentes[0][0] <= t:
        yield from drenar(pendentes[0][1])  # o link volta depois do último item
    while fila:
        yield proxima_ms, fila.popleft()
        proxima_ms += INTERVALO_REENVIO_MS
    return max(estado.inicio_ms + fim_ms, proxima_ms - INTERVALO_REENVIO_MS)  # reenvio passa do fim


def gerar(roteiro: Roteiro, opcoes: Opcoes = Opcoes()) -> Iterator[tuple[int, Item]]:
    """Linha do tempo do roteiro: pares (t simulado em ms, mensagem ou linha crua)."""
    agora_ms = 0
    for i, tentativa in enumerate(roteiro.tentativas):
        agora_ms += round(tentativa.pausa_s * 1000)
        inicio = tentativa.inicio or ("nova" if i == 0 else "retomada")
        estado = _Boot(roteiro, tentativa, opcoes, opcoes.boot + i, agora_ms)
        agora_ms = yield from _com_link(estado, _tentativa(estado, inicio))
