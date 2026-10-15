from datetime import UTC, datetime, timedelta

from app.services.monitor_conexao import MonitorConexao


def test_segundos_sem_sinal():
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    base = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor.registrar_mensagem(base)
    assert monitor.segundos_sem_sinal(base + timedelta(seconds=4)) == 4.0


def test_sem_mensagem_conta_desde_a_abertura_da_tentativa():
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    inicio = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    assert monitor.segundos_sem_sinal(inicio + timedelta(seconds=10), inicio=inicio) == 10.0


def test_mensagem_anterior_a_tentativa_nao_conta():
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    inicio = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor.registrar_mensagem(inicio - timedelta(seconds=30))
    assert monitor.segundos_sem_sinal(inicio + timedelta(seconds=3), inicio=inicio) == 3.0
