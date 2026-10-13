from app.repositories.telemetria import TelemetriaRepository


def test_contar_leituras_vazio(db_session):
    repo = TelemetriaRepository(db_session)
    assert repo.contar_leituras() == 0
