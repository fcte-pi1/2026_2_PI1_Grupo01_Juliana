from uuid import UUID

from app.db import get_db
from app.repositories import ExecucaoRepository, FiltroExecucoes
from app.schemas.api import (
    EncerrarTentativaRequest,
    ExecucaoDetalhe,
    NovaExecucaoRequest,
    Recusa,
    ResumoExecucao,
    RetornoNovaExecucao,
    TipoLabirinto,
)
from app.services import consulta_execucoes
from app.services.excecoes_execucao import (
    ExecucaoNaoEncontrada,
    RecusaTentativa,
    TentativaIndisponivel,
)
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.publicacao_eventos import publicar_apos_commit
from fastapi import APIRouter, Depends, HTTPException, Query, Request, Response, status
from fastapi.responses import JSONResponse
from sqlalchemy.orm import Session

router = APIRouter(prefix="/execucoes", tags=["execucoes"])

LIMITE_PADRAO = 50
LIMITE_MAXIMO = 100


def _gerenciador(request: Request) -> GerenciadorExecucoes:
    return request.app.state.gerenciador


def _resposta_recusa(recusa: RecusaTentativa) -> JSONResponse:
    corpo = Recusa(motivo=recusa.motivo, ocorrida_em=recusa.ocorrida_em).model_dump(mode="json")
    return JSONResponse(status_code=status.HTTP_409_CONFLICT, content=corpo)


@router.post(
    "",
    response_model=RetornoNovaExecucao,
    status_code=201,
    responses={409: {"model": Recusa}},
)
def criar_execucao(
    body: NovaExecucaoRequest,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> RetornoNovaExecucao | JSONResponse:
    try:
        retorno = gerenciador.nova_execucao(db, body.tipo_labirinto)
    except RecusaTentativa as recusa:
        return _resposta_recusa(recusa)
    db.commit()
    return RetornoNovaExecucao(
        execucao_id=retorno.execucao_id,
        tentativa_id=retorno.tentativa_id,
        attempt_index=retorno.attempt_index,
    )


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
def buscar_execucao_em_andamento(
    db: Session = Depends(get_db),
) -> ExecucaoDetalhe | Response:
    execucao = ExecucaoRepository(db).buscar_execucao_em_andamento()
    if execucao is None:
        return Response(status_code=status.HTTP_204_NO_CONTENT)
    detalhe = consulta_execucoes.buscar_detalhe(ExecucaoRepository(db), execucao.execucao_id)
    assert detalhe is not None
    return detalhe


@router.get("/{execucao_id}", response_model=ExecucaoDetalhe)
def obter_execucao(execucao_id: UUID, db: Session = Depends(get_db)) -> ExecucaoDetalhe:
    detalhe = consulta_execucoes.buscar_detalhe(ExecucaoRepository(db), execucao_id)
    if detalhe is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        )
    return detalhe


@router.post(
    "/{execucao_id}/encerrar-tentativa",
    response_model=ExecucaoDetalhe,
    responses={
        409: {
            "description": "Não há tentativa aberta ou a execução não está em andamento.",
        }
    },
)
def encerrar_tentativa(
    execucao_id: UUID,
    body: EncerrarTentativaRequest,
    request: Request,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> ExecucaoDetalhe:
    try:
        eventos = gerenciador.encerrar_tentativa(db, execucao_id, body.motivo, body.observacao)
    except ExecucaoNaoEncontrada:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        ) from None
    except TentativaIndisponivel:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Não há tentativa aberta para encerrar.",
        ) from None
    db.commit()
    publicar_apos_commit(request.app.state.publicador_sse, execucao_id, eventos)
    detalhe = consulta_execucoes.buscar_detalhe(ExecucaoRepository(db), execucao_id)
    assert detalhe is not None
    return detalhe


@router.post(
    "/{execucao_id}/retomar",
    response_model=RetornoNovaExecucao,
    status_code=201,
    responses={409: {"model": Recusa}},
)
def retomar_execucao(
    execucao_id: UUID,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> RetornoNovaExecucao | JSONResponse:
    try:
        retorno = gerenciador.retomar(db, execucao_id)
    except ExecucaoNaoEncontrada:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        ) from None
    except RecusaTentativa as recusa:
        db.commit()
        return _resposta_recusa(recusa)
    db.commit()
    return RetornoNovaExecucao(
        execucao_id=retorno.execucao_id,
        tentativa_id=retorno.tentativa_id,
        attempt_index=retorno.attempt_index,
    )
