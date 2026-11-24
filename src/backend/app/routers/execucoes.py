from typing import Literal
from uuid import UUID

from app.db import get_db
from app.repositories import ExecucaoRepository, FiltroExecucoes
from app.schemas.api import (
    EncerrarTentativaRequest,
    ExecucaoDetalhe,
    NovaExecucaoRequest,
    Recusa,
    ResumoCampanha,
    ResumoExecucao,
    RetomarRequest,
    RetornoNovaExecucao,
    TipoLabirinto,
)
from app.services import campanha, consulta_execucoes
from app.services.excecoes_execucao import (
    ExecucaoIndisponivel,
    ExecucaoNaoEncontrada,
    RecusaTentativa,
    TentativaIndisponivel,
)
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.publicacao_eventos import publicar_apos_commit
from fastapi import APIRouter, Depends, HTTPException, Query, Request, Response, status
from fastapi.responses import JSONResponse, PlainTextResponse
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
        retorno = gerenciador.nova_execucao(
            db, body.tipo_labirinto, body.configuracao, body.observacao, body.ignorar_bateria
        )
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
    configuracao: str | None = Query(default=None, max_length=60),
    limite: int = Query(default=LIMITE_PADRAO, ge=1, le=LIMITE_MAXIMO),
    offset: int = Query(default=0, ge=0),
    db: Session = Depends(get_db),
) -> list[ResumoExecucao]:
    resumos, total = consulta_execucoes.listar_historico(
        ExecucaoRepository(db),
        FiltroExecucoes(tipo_labirinto=tipo_labirinto, configuracao=configuracao),
        limite=limite,
        offset=offset,
    )
    response.headers["X-Total-Count"] = str(total)
    return resumos


@router.get("/campanha", response_model=ResumoCampanha)
def resumo_da_campanha(
    tipo_labirinto: TipoLabirinto | None = Query(default=None),
    configuracao: str | None = Query(default=None, max_length=60),
    db: Session = Depends(get_db),
) -> ResumoCampanha:
    execucoes = ExecucaoRepository(db).listar_todas(
        FiltroExecucoes(tipo_labirinto=tipo_labirinto, configuracao=configuracao)
    )
    return campanha.resumir(execucoes)


@router.get("/exportar")
def exportar_execucoes(
    formato: Literal["csv", "json"] = Query(default="csv"),
    tipo_labirinto: TipoLabirinto | None = Query(default=None),
    configuracao: str | None = Query(default=None, max_length=60),
    db: Session = Depends(get_db),
) -> Response:
    """Uma linha por tentativa, para planilha (csv) ou análise (json)."""
    execucoes = ExecucaoRepository(db).listar_todas(
        FiltroExecucoes(tipo_labirinto=tipo_labirinto, configuracao=configuracao)
    )
    linhas = campanha.linhas_exportacao(execucoes)
    cabecalho = {"Content-Disposition": f'attachment; filename="execucoes.{formato}"'}
    if formato == "json":
        return JSONResponse(content=linhas, headers=cabecalho)
    return PlainTextResponse(
        campanha.para_csv(linhas), media_type="text/csv; charset=utf-8", headers=cabecalho
    )


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
    "/{execucao_id}/encerrar",
    response_model=ExecucaoDetalhe,
    responses={
        409: {
            "description": "A execução não está em andamento.",
        }
    },
)
def encerrar_execucao(
    execucao_id: UUID,
    request: Request,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> ExecucaoDetalhe:
    try:
        eventos = gerenciador.encerrar_execucao(db, execucao_id)
    except ExecucaoNaoEncontrada:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        ) from None
    except ExecucaoIndisponivel:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="A execução não está em andamento.",
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
    body: RetomarRequest | None = None,
    db: Session = Depends(get_db),
    gerenciador: GerenciadorExecucoes = Depends(_gerenciador),
) -> RetornoNovaExecucao | JSONResponse:
    try:
        retorno = gerenciador.retomar(db, execucao_id, bool(body and body.ignorar_bateria))
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
