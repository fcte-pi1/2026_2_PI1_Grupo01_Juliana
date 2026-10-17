"""Varredura de perda de link e estouro de tempo fora do POST /telemetria (RF31, RF32)."""

from __future__ import annotations

import logging
import threading
from collections.abc import Callable

from app.db import SessionLocal
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from app.services.publicacao_eventos import publicar_apos_commit
from app.services.publicador_sse import PublicadorSSE
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")

INTERVALO_S = 1.0


def gravar_se_cancelou(
    sessao: Session,
    gerenciador: GerenciadorExecucoes,
    monitor: MonitorConexao,
    publicador: PublicadorSSE | None = None,
) -> bool:
    """Aplica os monitores e confirma a transação só quando houve cancelamento."""
    resultado = gerenciador.verificar_monitores(sessao, monitor)
    if not resultado:
        return False
    sessao.commit()
    publicar_apos_commit(publicador, resultado.execucao_id, resultado.eventos)
    return True


def ciclo(
    gerenciador: GerenciadorExecucoes,
    monitor: MonitorConexao,
    trava: threading.Lock,
    abrir_sessao: Callable[[], Session] = SessionLocal,
    publicador: PublicadorSSE | None = None,
) -> None:
    with trava:
        sessao = abrir_sessao()
        try:
            gravar_se_cancelou(sessao, gerenciador, monitor, publicador)
        except Exception:
            sessao.rollback()
            logger.exception("falha ao verificar monitores de link e tempo")
        finally:
            sessao.close()


def vigiar(stop: threading.Event, passar: Callable[[], None]) -> None:
    """Loop até `stop`. A primeira passada é imediata; as seguintes, a cada 1 s."""
    while not stop.is_set():
        try:
            passar()
        except Exception:
            logger.exception("falha ao verificar monitores de link e tempo")
        stop.wait(INTERVALO_S)
