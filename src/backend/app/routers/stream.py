"""Stream de eventos da execução ao vivo (BACK-07)."""

from uuid import UUID

from app.db import get_db
from app.repositories.execucoes import ExecucaoRepository
from app.repositories.telemetria import TelemetriaRepository
from app.services.fabrica_eventos import FABRICAS
from app.services.fluxo_sse import fluxo_eventos
from fastapi import APIRouter, Depends, Header, HTTPException, Request, status
from fastapi.concurrency import run_in_threadpool
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session

router = APIRouter(prefix="/execucoes", tags=["stream"])

CABECALHOS = {
    "Cache-Control": "no-cache, no-transform",
    # pede a proxies que não acumulem o corpo, senão o evento chega em lote
    "X-Accel-Buffering": "no",
}


def _ultimo_evento(valor: str | None) -> int:
    """`Last-Event-ID` como inteiro.

    Ausente, vazio, negativo ou não numérico vale 0, que reenvia o trajeto
    inteiro — o caso de quem abre a tela no meio da corrida. Por isso o header é
    recebido como texto: tipado como inteiro, um valor estranho viraria 422.
    """
    try:
        return max(int(valor), 0)
    except (TypeError, ValueError):
        return 0


@router.get("/{execucao_id}/stream", response_class=StreamingResponse)
async def stream_execucao(
    execucao_id: UUID,
    request: Request,
    last_event_id: str | None = Header(default=None, alias="Last-Event-ID"),
    db: Session = Depends(get_db),
) -> StreamingResponse:
    """Eventos da execução, no formato `text/event-stream` do openapi.

    Começa reenviando os passos acima do `Last-Event-ID` e segue com os eventos
    ao vivo, mais um comentário de keep-alive a cada 15 s. Se a execução já
    terminou, reenvia o trajeto e encerra o fluxo.

    Todo o acesso ao banco acontece aqui, antes de a resposta começar: o gerador
    não pode consultar nada, porque a sessão do `Depends` já estará fechada.
    """
    publicador = request.app.state.publicador_sse
    status_execucao = await run_in_threadpool(ExecucaoRepository(db).status_de, execucao_id)
    if status_execucao is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND, detail="Execução não encontrada."
        )

    # assina antes de consultar o reenvio: na ordem inversa, um passo commitado
    # entre a consulta e o início do consumo da fila se perderia sem deixar
    # rastro. Assim, no pior caso ele duplica, e o fluxo descarta a duplicata.
    assinatura = publicador.assinar(str(execucao_id))
    try:
        ultimo = _ultimo_evento(last_event_id)
        passos = await run_in_threadpool(
            TelemetriaRepository(db).listar_passos_depois, execucao_id, ultimo
        )
        reenvio = [FABRICAS.criar(passo) for passo in passos]
    except Exception:
        assinatura.fechar()
        raise

    return StreamingResponse(
        fluxo_eventos(
            assinatura, reenvio, encerrar_apos_reenvio=status_execucao != "em_andamento"
        ),
        media_type="text/event-stream",
        headers=CABECALHOS,
    )
