from decimal import Decimal

from app.services.calculo_metricas import distancia_celulas_para_metros, velocidade_media_ms


def test_distancia_tres_celulas():
    assert distancia_celulas_para_metros(3) == Decimal("0.54")


def test_velocidade_media():
    assert velocidade_media_ms(Decimal("0.54"), Decimal("3")) == Decimal("0.18")
