import logging
import threading
from contextlib import asynccontextmanager

from app.config import settings
from app.routers import back_health, execucoes, health_check, stream, telemetria
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.ingestao_telemetria import IngestaoTelemetria
from app.services.monitor_conexao import MonitorConexao
from app.services.publicador_sse import PublicadorSSE
from app.services.vigilancia_monitores import ciclo, vigiar
from contrato.telemetria import Deduplicador, RelogioDoRobo
from fastapi import FastAPI
from fastapi.openapi.utils import get_openapi


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
    app.state.gerenciador = GerenciadorExecucoes(
        settings.tempo_maximo_execucao_s, settings.validade_health_check_s
    )
    app.state.monitor_conexao = MonitorConexao(settings.limite_sem_sinal_s)
    app.state.publicador_sse = PublicadorSSE()
    app.state.deduplicador = Deduplicador()
    app.state.relogio_do_robo = RelogioDoRobo()
    app.state.ingestao_telemetria = IngestaoTelemetria(
        app.state.deduplicador,
        app.state.relogio_do_robo,
        app.state.monitor_conexao,
        app.state.publicador_sse,
    )

    # o POST /telemetria só vê o silêncio quando chega outra linha; esta tarefa cobre a ausência
    def passar() -> None:
        ciclo(
            app.state.gerenciador,
            app.state.monitor_conexao,
            app.state.ingestao_telemetria.trava,
            publicador=app.state.publicador_sse,
        )

    parar = threading.Event()
    varredura = threading.Thread(
        target=vigiar, args=(parar, passar), name="monitores-link-tempo", daemon=True
    )
    varredura.start()
    try:
        yield
    finally:
        parar.set()
        varredura.join(timeout=5)


app = FastAPI(
    title="Micromouse — Sistema de Telemetria",
    version="0.1.0",
    lifespan=lifespan,
)

app.include_router(back_health.router)
app.include_router(execucoes.router)
app.include_router(health_check.router)
app.include_router(telemetria.router)
app.include_router(stream.router)


def _publicar_422_como_oneof(esquema: dict) -> None:
    """O contrato usa oneOf: os dois corpos do 422 não cabem no mesmo esquema."""
    conteudo = esquema["paths"]["/telemetria"]["post"]["responses"]["422"]["content"]
    corpo = conteudo["application/json"]["schema"]
    corpo["oneOf"] = corpo.pop("anyOf")
    corpo.pop("title", None)


def _esquema_openapi() -> dict:
    if app.openapi_schema:
        return app.openapi_schema
    esquema = get_openapi(title=app.title, version=app.version, routes=app.routes)
    _publicar_422_como_oneof(esquema)
    app.openapi_schema = esquema
    return esquema


app.openapi = _esquema_openapi
