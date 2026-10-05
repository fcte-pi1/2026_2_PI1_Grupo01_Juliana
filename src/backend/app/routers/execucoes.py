from uuid import UUID

from app.routers.deps import nao_implementado
from app.schemas.api import (
    CorrigirTipoLabirintoRequest,
    EncerrarTentativaRequest,
    ExecucaoCriadaResponse,
    ExecucaoDetalhe,
    ExecucaoResumo,
    NovaExecucaoRequest,
    TipoLabirinto,
)
from fastapi import APIRouter, Query

router = APIRouter(prefix="/execucoes", tags=["execucoes"])


@router.post("", response_model=ExecucaoCriadaResponse, status_code=201)
def criar_execucao(body: NovaExecucaoRequest) -> ExecucaoCriadaResponse:
    nao_implementado()


@router.get("", response_model=list[ExecucaoResumo])
def listar_execucoes(
    tipo_labirinto: TipoLabirinto | None = Query(default=None),
) -> list[ExecucaoResumo]:
    nao_implementado()


@router.get("/{execucao_id}", response_model=ExecucaoDetalhe)
def obter_execucao(execucao_id: UUID) -> ExecucaoDetalhe:
    nao_implementado()


@router.post("/{execucao_id}/encerrar-tentativa", response_model=ExecucaoDetalhe)
def encerrar_tentativa(execucao_id: UUID, body: EncerrarTentativaRequest) -> ExecucaoDetalhe:
    nao_implementado()


@router.post("/{execucao_id}/retomar", response_model=ExecucaoCriadaResponse, status_code=201)
def retomar_execucao(execucao_id: UUID) -> ExecucaoCriadaResponse:
    nao_implementado()


@router.patch("/{execucao_id}/tipo-labirinto", response_model=ExecucaoDetalhe)
def corrigir_tipo_labirinto(
    execucao_id: UUID,
    body: CorrigirTipoLabirintoRequest,
) -> ExecucaoDetalhe:
    nao_implementado()
