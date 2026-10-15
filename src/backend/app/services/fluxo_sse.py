"""Frames e fluxo de eventos de um cliente do stream (BACK-07).

O gerador não toca no banco: o reenvio chega pronto, montado pelo router. Desde
o FastAPI 0.106 a sessão do `Depends(get_db)` é fechada antes do corpo da
resposta começar a ser enviado, então consultar daqui usaria sessão fechada.
"""

import asyncio
from collections.abc import AsyncIterator, Sequence

from app.services.publicador_sse import Assinatura, EventoSSE

INTERVALO_KEEPALIVE_S = 15.0
# comentário SSE: segura a conexão sem virar evento na tela
COMENTARIO_KEEPALIVE = ": keep-alive\n\n"


def montar_frame(evento: EventoSSE) -> str:
    """Frame no formato do openapi: `id:` só no passo, `event:` igual ao campo tipo."""
    linhas = []
    if evento.id is not None:
        linhas.append(f"id: {evento.id}")
    linhas.append(f"event: {evento.tipo}")
    linhas.append(f"data: {evento.dados}")
    return "\n".join(linhas) + "\n\n"


async def fluxo_eventos(
    assinatura: Assinatura,
    reenvio: Sequence[EventoSSE],
    *,
    encerrar_apos_reenvio: bool = False,
    intervalo_keepalive: float = INTERVALO_KEEPALIVE_S,
) -> AsyncIterator[str]:
    """Reenvio do trajeto, depois os eventos ao vivo da fila deste cliente."""
    try:
        # abre furando buffer de proxy e tornando o stream observável de imediato
        yield COMENTARIO_KEEPALIVE

        # ids que acabaram de ir no reenvio: a mesma cópia não vai duas vezes
        pendentes = {evento.id for evento in reenvio if evento.id is not None}
        for evento in reenvio:
            yield montar_frame(evento)

        if encerrar_apos_reenvio:
            return

        while True:
            try:
                evento = await asyncio.wait_for(assinatura.fila.get(), intervalo_keepalive)
            except TimeoutError:
                yield COMENTARIO_KEEPALIVE
                continue
            if evento is None:
                # sentinela do publicador: fila estourada, o cliente reconecta
                return
            if evento.id is not None and evento.id in pendentes:
                # consome só esta cópia; o id volta a valer para a próxima
                # publicação do mesmo passo, que é a releitura das paredes
                pendentes.discard(evento.id)
                continue
            yield montar_frame(evento)
            if evento.encerra:
                return
    finally:
        assinatura.fechar()
