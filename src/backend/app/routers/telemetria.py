from app.routers.deps import nao_implementado
from app.schemas.telemetria import MensagemTelemetria, RespostaTelemetria
from fastapi import APIRouter

router = APIRouter(prefix="/telemetria", tags=["telemetria"])


@router.post("", response_model=RespostaTelemetria)
def receber_telemetria(body: MensagemTelemetria) -> RespostaTelemetria:
    nao_implementado()
