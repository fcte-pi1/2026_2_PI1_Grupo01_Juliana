from datetime import timedelta

from app.db import get_db
from app.repositories import Repositorio
from app.schemas.api import SolicitacaoHealthCheck, UltimoHealthCheck
from app.services.consulta_execucoes import itens_health_check
from app.services.evento_health_check import (
    CANAL_HEALTH_CHECK,
    INTERVALO_RELOGIO_S,
    evento_health_check,
)
from app.services.excecoes_execucao import TentativaEmAndamento
from app.services.fluxo_sse import fluxo_eventos
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from fastapi import APIRouter, Depends, HTTPException, Request, Response, status
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session

CABECALHOS_STREAM = {
    "Cache-Control": "no-cache, no-transform",
    "X-Accel-Buffering": "no",
}

router = APIRouter(prefix="/health-check", tags=["health-check"])


def _gerenciador(request: Request) -> GerenciadorExecucoes:
    return request.app.state.gerenciador


@router.get("/ultimo", response_model=UltimoHealthCheck)
def buscar_ultimo_health_check(
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> UltimoHealthCheck | Response:
    health_check = Repositorio(db).ultimo_health_check()
    if health_check is None:
        return Response(status_code=status.HTTP_204_NO_CONTENT)
    valido_ate = None
    if health_check.aprovado and health_check.concluido_em is not None:
        valido_ate = health_check.concluido_em + timedelta(
            seconds=gerenciador.validade_health_check_s
        )
    return UltimoHealthCheck(
        aprovado=health_check.aprovado,
        tipo_dip=health_check.tipo_dip,
        iniciado_em=health_check.iniciado_em,
        concluido_em=health_check.concluido_em,
        valido_ate=valido_ate,
        itens=itens_health_check(health_check),
    )


@router.post(
    "/solicitar",
    response_model=SolicitacaoHealthCheck,
    status_code=status.HTTP_202_ACCEPTED,
)
def solicitar_health_check(
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> SolicitacaoHealthCheck:
    try:
        solicitado_em = gerenciador.solicitar_health_check(db)
    except TentativaEmAndamento as erro:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Há uma tentativa em execução; encerre-a antes de pedir um novo health-check.",
        ) from erro
    return SolicitacaoHealthCheck(solicitado_em=solicitado_em)


@router.get("/stream")
async def stream_health_check(
    request: Request,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> StreamingResponse:
    """Retrato atual e, em seguida, cada atualização do robô.

    O primeiro evento repete o GET /health-check/ultimo. O relógio de 1 s começa
    com essa abertura: é o keep-alive enquanto o próximo item não chega.
    """
    publicador = request.app.state.publicador_sse
    assinatura = publicador.assinar(CANAL_HEALTH_CHECK)
    try:
        health_check = await run_in_threadpool(Repositorio(db).ultimo_health_check)
        reenvio = (
            [evento_health_check(health_check, gerenciador.validade_health_check_s)]
            if health_check is not None
            else []
        )
    except Exception:
        assinatura.fechar()
        raise
    return StreamingResponse(
        fluxo_eventos(assinatura, reenvio, intervalo_keepalive=INTERVALO_RELOGIO_S),
        media_type="text/event-stream",
        headers=CABECALHOS_STREAM,
    )
