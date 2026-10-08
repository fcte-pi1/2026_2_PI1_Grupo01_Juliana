from uuid import UUID

from app.routers.deps import nao_implementado
from app.schemas.api import (
    EncerrarTentativaRequest,
    ExecucaoDetalhe,
    NovaExecucaoRequest,
    ResumoExecucao,
    RetornoNovaExecucao,
    TipoLabirinto,
)
from fastapi import APIRouter, Query

router = APIRouter(prefix="/execucoes", tags=["execucoes"])


@router.post("", response_model=RetornoNovaExecucao, status_code=201)
def criar_execucao(body: NovaExecucaoRequest) -> RetornoNovaExecucao:
    nao_implementado()


@router.get("", response_model=list[ResumoExecucao])
def listar_execucoes(
    tipo_labirinto: TipoLabirinto | None = Query(default=None),
) -> list[ResumoExecucao]:
    nao_implementado()


@router.get("/em-andamento", response_model=ExecucaoDetalhe)
def buscar_execucao_em_andamento() -> ExecucaoDetalhe:
    nao_implementado()


@router.get("/{execucao_id}", response_model=ExecucaoDetalhe)
def obter_execucao(execucao_id: UUID) -> ExecucaoDetalhe:
    nao_implementado()


@router.post("/{execucao_id}/encerrar-tentativa", response_model=ExecucaoDetalhe)
def encerrar_tentativa(execucao_id: UUID, body: EncerrarTentativaRequest) -> ExecucaoDetalhe:
    nao_implementado()


@router.post("/{execucao_id}/retomar", response_model=RetornoNovaExecucao, status_code=201)
def retomar_execucao(execucao_id: UUID) -> RetornoNovaExecucao:
    nao_implementado()
