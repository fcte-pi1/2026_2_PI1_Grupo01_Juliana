from app.routers.deps import nao_implementado
from contrato.telemetria import EntradaPonte, RespostaPonte
from fastapi import APIRouter

router = APIRouter(prefix="/telemetria", tags=["telemetria"])


@router.post("", response_model=RespostaPonte)
def receber_telemetria(body: EntradaPonte) -> RespostaPonte:
    nao_implementado()
