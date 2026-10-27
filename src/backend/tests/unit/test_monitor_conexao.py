from datetime import UTC, datetime, timedelta

from app.services.monitor_conexao import MonitorConexao


def test_segundos_sem_sinal():
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    base = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor.registrar_mensagem(base)
    assert monitor.segundos_sem_sinal(base + timedelta(seconds=4)) == 4.0


def test_sem_mensagem_conta_desde_a_abertura_da_tentativa():
    inicio = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor = MonitorConexao(limite_sem_sinal_s=10, relogio=lambda: inicio)
    assert monitor.segundos_sem_sinal(inicio + timedelta(seconds=10), inicio=inicio) == 10.0


def test_mensagem_anterior_a_tentativa_nao_conta():
    inicio = datetime(2026, 1, 1, 12, 0, 0, tzinfo=UTC)
    monitor = MonitorConexao(limite_sem_sinal_s=10, relogio=lambda: inicio)
    monitor.registrar_mensagem(inicio - timedelta(seconds=30))
    assert monitor.segundos_sem_sinal(inicio + timedelta(seconds=3), inicio=inicio) == 3.0


def test_tentativa_anterior_ao_boot_da_api_tem_uma_janela_para_reconectar():
    reinicio = datetime(2026, 1, 1, 12, 1, 0, tzinfo=UTC)
    inicio_da_tentativa = reinicio - timedelta(minutes=5)
    monitor = MonitorConexao(limite_sem_sinal_s=10, relogio=lambda: reinicio)

    assert (
        monitor.segundos_sem_sinal(reinicio + timedelta(seconds=9), inicio=inicio_da_tentativa)
        == 9.0
    )
    assert (
        monitor.segundos_sem_sinal(reinicio + timedelta(seconds=10), inicio=inicio_da_tentativa)
        == 10.0
    )
