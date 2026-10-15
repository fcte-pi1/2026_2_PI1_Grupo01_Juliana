from app.db import get_db
from contrato.telemetria import Comando, EntradaPonte, LinhaInvalida, RespostaPonte
from fastapi import APIRouter, Depends, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, ConfigDict, Field
from sqlalchemy.orm import Session

router = APIRouter(prefix="/telemetria", tags=["telemetria"])

RESUMO = "Receber uma linha de telemetria da ponte"
DESCRICAO = (
    "Corpo **EntradaPonte** (4.4.1): a ponte repassa a linha crua lida da serial "
    "e o instante `recebido_em` com fuso horário. Toda validação do JSON serial v1 "
    "fica no backend. Linha aceita ou descartada só em log (seq repetido, sem tentativa "
    "aberta, hc_item fora do health-check) responde 200. Após **Encerrar tentativa**, "
    "a resposta inclui o comando de interrupção em `comandos` em todo POST até a `falha` "
    "com `origem` = `web` ou nova tentativa; a ponte escreve cada item na serial sem traduzir."
)
DESCRICAO_422 = (
    "Dois casos. Linha inválida (mais de 256 B, não é JSON, v diferente de 1, tipo "
    "desconhecido, campo faltando ou fora da faixa, falha incoerente, célula fora do "
    "labirinto): o corpo traz `detail` em texto e `comandos`, que a ponte escreve na "
    "serial como no 200. Corpo **EntradaPonte** malformado (não é JSON, falta `linha`, "
    "por exemplo): formato padrão do FastAPI, com `detail` em lista e sem `comandos`. "
    "A ponte não deve reenviar 4xx."
)
EXEMPLOS_422 = {
    "linha-invalida": {
        "summary": "Linha recusada sem downlink pendente",
        "value": {"detail": "linha com 300 bytes (máximo 256)", "comandos": []},
    },
    "linha-invalida-interrupcao-pendente": {
        "summary": "Linha recusada depois de encerrar a tentativa manualmente",
        "value": {
            "detail": "linha com 300 bytes (máximo 256)",
            "comandos": [{"v": 1, "cmd": "interromper"}],
        },
    },
    "corpo-nao-json": {
        "summary": "Corpo que não é JSON",
        "value": {
            "detail": [
                {
                    "type": "json_invalid",
                    "loc": ["body", 1],
                    "msg": "JSON decode error",
                    "input": {},
                    "ctx": {"error": "Expecting property name enclosed in double quotes"},
                }
            ]
        },
    },
    "corpo-sem-linha": {
        "summary": "EntradaPonte sem o campo linha",
        "value": {
            "detail": [
                {
                    "type": "missing",
                    "loc": ["body", "linha"],
                    "msg": "Field required",
                    "input": {"recebido_em": "2026-10-13T17:00:00.000-03:00"},
                }
            ]
        },
    },
}


class ErroLinhaInvalida(BaseModel):
    """Linha recusada: motivo em texto e comandos para a ponte escrever na serial."""

    detail: str = Field(description="Motivo da recusa da linha, só para log.")
    comandos: list[Comando]


class ItemErroValidacao(BaseModel):
    """Um item de `detail` no 422 padrão do FastAPI."""

    model_config = ConfigDict(extra="allow")

    loc: list[str | int] = Field(
        description=(
            "Caminho do erro; o FastAPI usa inteiros para posições (por exemplo, "
            "o caractere em que o JSON do corpo quebrou)."
        )
    )
    msg: str
    type: str


class ErroValidacao(BaseModel):
    """Corpo EntradaPonte malformado, no formato padrão do FastAPI."""

    detail: list[ItemErroValidacao]


@router.post(
    "",
    response_model=RespostaPonte,
    summary=RESUMO,
    description=DESCRICAO,
    responses={
        422: {
            "model": ErroLinhaInvalida | ErroValidacao,
            "description": DESCRICAO_422,
            "content": {"application/json": {"examples": EXEMPLOS_422}},
        }
    },
)
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
