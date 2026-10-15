from datetime import UTC, datetime, timedelta
from decimal import Decimal

from app.services.calculo_metricas import (
    consumo_bateria_mv,
    consumo_bateria_pct,
    contar_celulas_unicas,
    distancia_celulas_para_metros,
    percentual_carga_mv,
    tempo_decorrido_s,
    variacao_melhor_tempo_pct,
    velocidade_media_ms,
    velocidade_media_por_celulas,
)


def test_distancia_tres_celulas():
    assert distancia_celulas_para_metros(3) == Decimal("0.54")


def test_velocidade_media():
    assert velocidade_media_ms(Decimal("0.54"), Decimal("3")) == Decimal("0.18")


def test_velocidade_media_tempo_zero():
    assert velocidade_media_ms(Decimal("0.54"), Decimal("0")) is None
    assert velocidade_media_ms(Decimal("0.54"), Decimal("-1")) is None


def test_velocidade_media_por_celulas():
    assert velocidade_media_por_celulas(3, Decimal("3")) == Decimal("0.18")
    assert velocidade_media_por_celulas(0, Decimal("3")) is None


def test_contar_celulas_unicas():
    assert contar_celulas_unicas([(0, 0), (0, 1), (0, 1), (1, 1)]) == 3


def test_tempo_decorrido_s():
    inicio = datetime(2026, 10, 7, 14, 0, 0, tzinfo=UTC)
    fim = inicio + timedelta(seconds=2, milliseconds=500)
    assert tempo_decorrido_s(inicio, fim) == Decimal("2.5")


def test_percentual_carga_mv_pontos_exatos_lipo():
    assert percentual_carga_mv(8400) == Decimal("100")
    assert percentual_carga_mv(7800) == Decimal("55")
    assert percentual_carga_mv(7000) == Decimal("0")


def test_consumo_bateria_mv():
    assert consumo_bateria_mv(Decimal("8400"), Decimal("7800")) == Decimal("600")
    assert consumo_bateria_mv(None, Decimal("7800")) is None


def test_consumo_bateria_pct_pontos_exatos():
    assert consumo_bateria_pct(Decimal("8400"), Decimal("7800")) == Decimal("45")


def test_variacao_melhor_tempo():
    assert variacao_melhor_tempo_pct(Decimal("30"), Decimal("40")) == Decimal("-25")


def test_variacao_sem_historico():
    assert variacao_melhor_tempo_pct(Decimal("30"), None) is None


def test_variacao_melhor_zero_ou_negativo():
    assert variacao_melhor_tempo_pct(Decimal("30"), Decimal("0")) is None
    assert variacao_melhor_tempo_pct(Decimal("30"), Decimal("-5")) is None
