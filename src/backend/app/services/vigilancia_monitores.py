"""Varredura de perda de link e estouro de tempo fora do POST /telemetria (RF31, RF32)."""

from __future__ import annotations

import logging
import threading
from collections.abc import Callable

from app.db import SessionLocal
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")

INTERVALO_S = 1.0


def gravar_se_cancelou(
    sessao: Session, gerenciador: GerenciadorExecucoes, monitor: MonitorConexao
) -> bool:
    """Aplica os monitores e confirma a transação só quando houve cancelamento."""
    if not gerenciador.verificar_monitores(sessao, monitor):
        return False
    sessao.commit()
    return True


def ciclo(
    gerenciador: GerenciadorExecucoes,
    monitor: MonitorConexao,
    trava: threading.Lock,
    abrir_sessao: Callable[[], Session] = SessionLocal,
) -> None:
    with trava:
        sessao = abrir_sessao()
        try:
            gravar_se_cancelou(sessao, gerenciador, monitor)
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
