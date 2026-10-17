"""Monitor de link e tempo sem telemetria (BACK-05)."""

from collections.abc import Callable
from datetime import UTC, datetime

Relogio = Callable[[], datetime]


def relogio_do_sistema() -> datetime:
    return datetime.now(UTC)


class MonitorConexao:
    def __init__(self, limite_sem_sinal_s: int, relogio: Relogio = relogio_do_sistema) -> None:
        self.limite_sem_sinal_s = limite_sem_sinal_s
        # injetável: os testes avançam um relógio falso em vez de esperar 10 s
        self.relogio = relogio
        self._ultima_mensagem: datetime | None = None

    def agora(self) -> datetime:
        return self.relogio()

    def registrar_mensagem(self, recebido_em: datetime | None = None) -> None:
        self._ultima_mensagem = recebido_em or self.agora()

    def segundos_sem_sinal(
        self, agora: datetime | None = None, *, inicio: datetime | None = None
    ) -> float | None:
        """Tempo desde a última mensagem desta tentativa.

        Sem mensagem depois de `inicio`, a contagem começa na abertura da
        tentativa. Mensagem anterior a `inicio` é de outra execução e não conta.
        """
        referencia = agora or self.agora()
        ultima = self._ultima_mensagem
        if inicio is not None and (ultima is None or ultima < inicio):
            ancora = inicio
        elif ultima is None:
            return None
        else:
            ancora = ultima
        return (referencia - ancora).total_seconds()
