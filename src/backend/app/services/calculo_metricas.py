"""Calculo de metricas de execucao (BACK-04)."""

from __future__ import annotations

from datetime import datetime
from decimal import Decimal

LADO_CELULA_MM = 180
BATERIA_LIMIAR_FONTE_MV = 6700

# Espelho de BATERIA_CURVA_LIPO em src/firmware/include/config/robo.h
CURVA_LIPO_MV_PCT: tuple[tuple[int, int], ...] = (
    (8400, 100),
    (8200, 90),
    (8000, 75),
    (7800, 55),
    (7600, 35),
    (7400, 15),
    (7200, 5),
    (7000, 0),
)

# Espelho de BATERIA_CURVA_AA em src/firmware/include/config/robo.h
CURVA_AA_MV_PCT: tuple[tuple[int, int], ...] = (
    (6400, 100),
    (6000, 80),
    (5600, 50),
    (5200, 20),
    (5000, 0),
)


def distancia_celulas_para_metros(
    num_celulas: int, lado_celula_mm: int = LADO_CELULA_MM
) -> Decimal:
    return Decimal(num_celulas) * Decimal(lado_celula_mm) / Decimal(1000)


def velocidade_media_ms(distancia_m: Decimal, tempo_s: Decimal) -> Decimal | None:
    if tempo_s <= 0:
        return None
    return distancia_m / tempo_s


def velocidade_media_por_celulas(num_celulas: int, tempo_s: Decimal) -> Decimal | None:
    if num_celulas <= 0:
        return None
    return velocidade_media_ms(distancia_celulas_para_metros(num_celulas), tempo_s)


def contar_deslocamentos(coordenadas: list[tuple[int, int]]) -> int:
    """Conta transições entre células; a posição inicial não é deslocamento."""
    return max(len(coordenadas) - 1, 0)


def tempo_decorrido_s(inicio: datetime, fim: datetime) -> Decimal:
    return Decimal(str((fim - inicio).total_seconds()))


def percentual_carga_mv(
    mv: Decimal | int,
    curva: tuple[tuple[int, int], ...] | None = None,
) -> Decimal:
    valor = int(mv)
    curva = curva or curva_bateria_por_tensao_inicial(valor)
    if valor >= curva[0][0]:
        return Decimal(curva[0][1])
    if valor <= curva[-1][0]:
        return Decimal(curva[-1][1])
    for (mv_alto, pct_alto), (mv_baixo, pct_baixo) in zip(curva, curva[1:], strict=False):
        if mv_baixo <= valor <= mv_alto:
            if mv_alto == mv_baixo:
                return Decimal(pct_alto)
            frac = Decimal(valor - mv_baixo) / Decimal(mv_alto - mv_baixo)
            return Decimal(pct_baixo) + frac * Decimal(pct_alto - pct_baixo)
    return Decimal(curva[-1][1])


def curva_bateria_por_tensao_inicial(
    inicial_mv: Decimal | int,
) -> tuple[tuple[int, int], ...]:
    """Seleciona uma vez por tentativa, com o mesmo limiar usado pelo firmware."""
    if int(inicial_mv) >= BATERIA_LIMIAR_FONTE_MV:
        return CURVA_LIPO_MV_PCT
    return CURVA_AA_MV_PCT


def consumo_bateria_mv(inicial_mv: Decimal | None, atual_mv: Decimal | None) -> Decimal | None:
    if inicial_mv is None or atual_mv is None:
        return None
    return inicial_mv - atual_mv


def consumo_bateria_pct(
    inicial_mv: Decimal | None,
    atual_mv: Decimal | None,
    curva: tuple[tuple[int, int], ...] | None = None,
) -> Decimal | None:
    if inicial_mv is None or atual_mv is None:
        return None
    curva = curva or curva_bateria_por_tensao_inicial(inicial_mv)
    return percentual_carga_mv(inicial_mv, curva) - percentual_carga_mv(atual_mv, curva)


def variacao_melhor_tempo_pct(
    tempo_atual: Decimal | None,
    melhor_anterior: Decimal | None,
) -> Decimal | None:
    if tempo_atual is None or melhor_anterior is None or melhor_anterior <= 0:
        return None
    return (tempo_atual - melhor_anterior) / melhor_anterior * Decimal(100)
