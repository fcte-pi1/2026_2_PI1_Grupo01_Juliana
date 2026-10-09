from app.db import get_db
from contrato.telemetria import EntradaPonte, LinhaInvalida, RespostaPonte
from fastapi import APIRouter, Depends, HTTPException, Request
from sqlalchemy.orm import Session

router = APIRouter(prefix="/telemetria", tags=["telemetria"])


@router.post("", response_model=RespostaPonte)
def receber_telemetria(
    body: EntradaPonte, request: Request, db: Session = Depends(get_db)
) -> RespostaPonte:
    estado = request.app.state
    try:
        return estado.ingestao_telemetria.processar(db, body, estado.gerenciador)
    except LinhaInvalida as erro:
        raise HTTPException(status_code=422, detail=str(erro)) from erro
