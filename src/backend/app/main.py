import logging
from contextlib import asynccontextmanager

from app.config import settings
from app.routers import back_health, execucoes, stream, telemetria
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.ingestao_telemetria import IngestaoTelemetria
from app.services.monitor_conexao import MonitorConexao
from app.services.publicador_sse import PublicadorSSE
from contrato.telemetria import Deduplicador, RelogioDoRobo
from fastapi import FastAPI


def _configurar_logs() -> None:
    """Mostra os INFO do `app.*`, como os descartes da telemetria (RNF-B09)."""
    logger = logging.getLogger("app")
    if not logger.handlers:
        saida = logging.StreamHandler()
        saida.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s: %(message)s"))
        logger.addHandler(saida)
    logger.setLevel(logging.INFO)


_configurar_logs()


@asynccontextmanager
async def lifespan(app: FastAPI):
    app.state.gerenciador = GerenciadorExecucoes(settings.tempo_maximo_execucao_s)
    app.state.monitor_conexao = MonitorConexao(settings.limite_sem_sinal_s)
    app.state.publicador_sse = PublicadorSSE()
    app.state.deduplicador = Deduplicador()
    app.state.relogio_do_robo = RelogioDoRobo()
    app.state.ingestao_telemetria = IngestaoTelemetria(
        app.state.deduplicador, app.state.relogio_do_robo, app.state.monitor_conexao
    )
    yield


app = FastAPI(
    title="Micromouse — Sistema de Telemetria",
    version="0.1.0",
    lifespan=lifespan,
)

app.include_router(back_health.router)
app.include_router(execucoes.router)
app.include_router(telemetria.router)
app.include_router(stream.router)
