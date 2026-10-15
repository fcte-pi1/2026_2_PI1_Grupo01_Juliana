"""Monitor de link e tempo sem telemetria (BACK-05)."""

from datetime import UTC, datetime


class MonitorConexao:
    def __init__(self, limite_sem_sinal_s: int) -> None:
        self.limite_sem_sinal_s = limite_sem_sinal_s
        self._ultima_mensagem: datetime | None = None

    def registrar_mensagem(self, recebido_em: datetime | None = None) -> None:
        self._ultima_mensagem = recebido_em or datetime.now(UTC)

    def segundos_sem_sinal(
        self, agora: datetime | None = None, *, inicio: datetime | None = None
    ) -> float | None:
        """Tempo desde a última mensagem desta tentativa.

        Sem mensagem depois de `inicio`, a contagem começa na abertura da
        tentativa. Mensagem anterior a `inicio` é de outra execução e não conta.
        """
        referencia = agora or datetime.now(UTC)
        ultima = self._ultima_mensagem
        if inicio is not None and (ultima is None or ultima < inicio):
            ancora = inicio
        elif ultima is None:
            return None
        else:
            ancora = ultima
        return (referencia - ancora).total_seconds()
