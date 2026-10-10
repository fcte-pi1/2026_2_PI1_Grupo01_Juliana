"""Máquina de estados da execução lógica e das tentativas (BACK-03)."""

import logging

from app.models import Tentativa
from contrato.telemetria import Comando, Mensagem
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")


class GerenciadorExecucoes:
    limite_tentativas: int = 3

    def __init__(self, tempo_maximo_execucao_s: int) -> None:
        self.tempo_maximo_execucao_s = tempo_maximo_execucao_s

    def aplicar_evento(
        self, sessao: Session, tentativa: Tentativa | None, mensagem: Mensagem
    ) -> None:
        """Recebe hc_resultado, falha e sucesso já validados (transições no BACK-03)."""
        logger.info(
            "evento %s recebido (tentativa %s)",
            mensagem.tipo,
            tentativa.tentativa_id if tentativa is not None else None,
        )

    def comandos_pendentes(self) -> list[Comando]:
        """Comandos para a ponte escrever na serial (interrupção no BACK-03)."""
        return []
