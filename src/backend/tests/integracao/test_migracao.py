from app.models import (
    ExecucaoLogica,
    Falha,
    HealthCheckItem,
    Labirinto,
    LeituraTelemetria,
    ParedeCelula,
    PassoTrajeto,
    Tentativa,
    TentativaRejeitada,
)
from sqlalchemy import inspect, text


def test_nove_tabelas_registradas_no_metadata():
    nomes = {
        Labirinto.__tablename__,
        ExecucaoLogica.__tablename__,
        Tentativa.__tablename__,
        LeituraTelemetria.__tablename__,
        PassoTrajeto.__tablename__,
        ParedeCelula.__tablename__,
        HealthCheckItem.__tablename__,
        Falha.__tablename__,
        TentativaRejeitada.__tablename__,
    }
    assert len(nomes) == 9


def test_indice_parcial_tentativa_aberta(engine):
    with engine.connect() as conn:
        row = conn.execute(
            text(
                """
                SELECT indexdef
                FROM pg_indexes
                WHERE tablename = 'tentativa'
                  AND indexname = 'uq_tentativa_aberta_por_execucao'
                """
            )
        ).first()
    assert row is not None
    assert "health-check" in row.indexdef
    assert "running" in row.indexdef


def test_labirintos_seed(engine):
    insp = inspect(engine)
    assert "labirinto" in insp.get_table_names()
    with engine.connect() as conn:
        count = conn.execute(text("SELECT COUNT(*) FROM labirinto")).scalar_one()
    assert count == 3
