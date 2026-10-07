from datetime import UTC, datetime, timedelta

from app.services.monitor_conexao import MonitorConexao


def test_segundos_sem_sinal():
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    base = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor.registrar_mensagem(base)
    assert monitor.segundos_sem_sinal(base + timedelta(seconds=4)) == 4.0
