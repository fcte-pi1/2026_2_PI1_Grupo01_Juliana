from app.config import Settings


def test_limites_tap_padrao():
    cfg = Settings.model_validate({"database_url": "postgresql+psycopg://u:p@localhost/db"})
    assert cfg.tempo_maximo_execucao_s == 600
    assert cfg.limite_sem_sinal_s == 10
