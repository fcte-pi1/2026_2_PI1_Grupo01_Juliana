"""Máquina de estados da execução lógica e das tentativas (BACK-03)."""

import logging

from app.models import Tentativa
from app.services.publicador_sse import EventoSSE
from contrato.telemetria import Comando, Mensagem
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")


class GerenciadorExecucoes:
    limite_tentativas: int = 3

    def __init__(self, tempo_maximo_execucao_s: int) -> None:
        self.tempo_maximo_execucao_s = tempo_maximo_execucao_s

    def aplicar_evento(
        self, sessao: Session, tentativa: Tentativa | None, mensagem: Mensagem
    ) -> list[EventoSSE]:
        """Recebe hc_resultado, falha e sucesso já validados (transições no BACK-03).

        Devolve os eventos SSE das transições, montados antes do COMMIT; quem
        publica é a ingestão, depois dele. Enquanto as transições não existem não
        há o que publicar: `status_execucao`, `attempt_index` e `tipo_dip` só
        passam a valer depois delas, e deduzi-los da mensagem serial mentiria para
        a tela. O BACK-03 monta `OrigemStatus`, `OrigemFalha` ou `OrigemSucesso` de
        `app.services.fabrica_eventos` e devolve `criar_eventos(origem)`.
        """
        logger.info(
            "evento %s recebido (tentativa %s)",
            mensagem.tipo,
            tentativa.tentativa_id if tentativa is not None else None,
        )
        return []

    def comandos_pendentes(self) -> list[Comando]:
        """Comandos para a ponte escrever na serial (interrupção no BACK-03)."""
        return []
