from uuid import UUID

from app.db import get_db
from app.repositories import ExecucaoRepository, FiltroExecucoes
from app.routers.deps import nao_implementado
from app.schemas.api import (
    EncerrarTentativaRequest,
    ExecucaoDetalhe,
    NovaExecucaoRequest,
    ResumoExecucao,
    RetornoNovaExecucao,
    TipoLabirinto,
)
from app.services import consulta_execucoes
from fastapi import APIRouter, Depends, HTTPException, Query, Response, status
from sqlalchemy.orm import Session

router = APIRouter(prefix="/execucoes", tags=["execucoes"])

LIMITE_PADRAO = 50
LIMITE_MAXIMO = 100


@router.post("", response_model=RetornoNovaExecucao, status_code=201)
def criar_execucao(body: NovaExecucaoRequest) -> RetornoNovaExecucao:
    nao_implementado()


@router.get("", response_model=list[ResumoExecucao])
def listar_execucoes(
    response: Response,
    tipo_labirinto: TipoLabirinto | None = Query(default=None),
    limite: int = Query(default=LIMITE_PADRAO, ge=1, le=LIMITE_MAXIMO),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[ResumoExecucao]:
    resumos, total = consulta_execucoes.listar_historico(
        ExecucaoRepository(db),
        FiltroExecucoes(tipo_labirinto=tipo_labirinto),
        limite=limite,
        offset=offset,
    )
    response.headers["X-Total-Count"] = str(total)
    return resumos


@router.get("/em-andamento", response_model=ExecucaoDetalhe)
def buscar_execucao_em_andamento() -> ExecucaoDetalhe:
    nao_implementado()


@router.get("/{execucao_id}", response_model=ExecucaoDetalhe)
def obter_execucao(execucao_id: UUID, db: Session = Depends(get_db)) -> ExecucaoDetalhe:
    detalhe = consulta_execucoes.buscar_detalhe(ExecucaoRepository(db), execucao_id)
    if detalhe is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        )
    return detalhe


@router.post("/{execucao_id}/encerrar-tentativa", response_model=ExecucaoDetalhe)
def encerrar_tentativa(execucao_id: UUID, body: EncerrarTentativaRequest) -> ExecucaoDetalhe:
    nao_implementado()


@router.post("/{execucao_id}/retomar", response_model=RetornoNovaExecucao, status_code=201)
def retomar_execucao(execucao_id: UUID) -> RetornoNovaExecucao:
    nao_implementado()
