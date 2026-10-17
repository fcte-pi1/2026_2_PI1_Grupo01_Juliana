from datetime import timedelta

from app.db import get_db
from app.repositories import Repositorio
from app.schemas.api import UltimoHealthCheck
from app.services.consulta_execucoes import itens_health_check
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from fastapi import APIRouter, Depends, Request, Response, status
from sqlalchemy.orm import Session

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
