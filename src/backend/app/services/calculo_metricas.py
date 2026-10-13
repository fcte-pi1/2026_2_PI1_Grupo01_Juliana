"""Cálculo de consumo, velocidade média, tempo e comparação (BACK-04)."""

from decimal import Decimal


def distancia_celulas_para_metros(num_celulas: int, lado_celula_mm: int = 180) -> Decimal:
    return Decimal(num_celulas) * Decimal(lado_celula_mm) / Decimal(1000)


def velocidade_media_ms(distancia_m: Decimal, tempo_s: Decimal) -> Decimal | None:
    if tempo_s <= 0:
        return None
    return distancia_m / tempo_s
