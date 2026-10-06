"""Gerador: transforma o roteiro numa linha do tempo de mensagens do contrato.

Função pura, sem relógio nem I/O: cada item sai com o instante simulado em que
deve ser enviado. Quem dorme, acelera e escreve é o emissor.
"""

from collections.abc import Iterator
from dataclasses import dataclass
from typing import get_args

from contrato.telemetria import (
    VERSAO,
    Componente,
    HcItem,
    HcResultado,
    Mensagem,
    Tel,
)

from simulador.roteiro import Roteiro, Tentativa

COMPONENTES: tuple[Componente, ...] = get_args(Componente)  # ordem da Tabela 3

PERIODO_TEL_PARADO_MS = 1000  # tel a 1 Hz parado
INTERVALO_HC_ITEM_MS = 200  # entre um teste de componente e o próximo
DURACAO_HC_MS = 2000  # hc_item em 100, 300, ..., 1700 ms; hc_resultado em 1900 ms


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

    def __init__(self, roteiro: Roteiro, tentativa: Tentativa, boot: int, inicio_ms: int):
        self.roteiro = roteiro
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
    """9 hc_item e o hc_resultado, com tel a 1 Hz em estado health-check."""
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


def gerar(roteiro: Roteiro, opcoes: Opcoes = Opcoes()) -> Iterator[tuple[int, Item]]:
    """Linha do tempo do roteiro: pares (t simulado em ms, mensagem ou linha crua)."""
    agora_ms = 0
    for i, tentativa in enumerate(roteiro.tentativas):
        agora_ms += round(tentativa.pausa_s * 1000)
        inicio = tentativa.inicio or ("nova" if i == 0 else "retomada")
        estado = _Boot(roteiro, tentativa, opcoes.boot + i, agora_ms)
        aprovado = yield from _health_check(estado, inicio)
        agora_ms += DURACAO_HC_MS
        if not aprovado:
            continue  # health-check reprovado: a tentativa termina aqui
