from app.db import get_db
from contrato.telemetria import EntradaPonte, LinhaInvalida, RespostaPonte
from fastapi import APIRouter, Depends, Request
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

router = APIRouter(prefix="/telemetria", tags=["telemetria"])


@router.post("", response_model=RespostaPonte)
def receber_telemetria(
    body: EntradaPonte, request: Request, db: Session = Depends(get_db)
) -> RespostaPonte | JSONResponse:
    estado = request.app.state
    try:
        return estado.ingestao_telemetria.processar(db, body, estado.gerenciador)
    except LinhaInvalida as erro:
        # a interrupção vai em toda resposta, inclusive na 422 (4.4.1)
        comandos = RespostaPonte(comandos=estado.gerenciador.comandos_pendentes())
        return JSONResponse(
            status_code=422, content={"detail": str(erro), **comandos.model_dump(mode="json")}
        )
