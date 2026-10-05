"""Monitor de link e tempo sem telemetria (BACK-05)."""

from datetime import UTC, datetime


class MonitorConexao:
    def __init__(self, limite_sem_sinal_s: int) -> None:
        self.limite_sem_sinal_s = limite_sem_sinal_s
        self._ultima_mensagem: datetime | None = None

    def registrar_mensagem(self, recebido_em: datetime | None = None) -> None:
        self._ultima_mensagem = recebido_em or datetime.now(UTC)

    def segundos_sem_sinal(self, agora: datetime | None = None) -> float | None:
        if self._ultima_mensagem is None:
            return None
        referencia = agora or datetime.now(UTC)
        return (referencia - self._ultima_mensagem).total_seconds()
