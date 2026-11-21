from app.schemas.api import EstadoRobo, PedidoReconexao, RelatoPonte, RespostaRelatoPonte
from fastapi import APIRouter, HTTPException, Request, status

router = APIRouter(prefix="/robo", tags=["robo"])


@router.get(
    "/estado",
    response_model=EstadoRobo,
    summary="Último sinal do robô e estado do enlace da ponte",
    description=(
        "Instante da última linha que a ponte entregou e há quantos segundos foi, mais o "
        "estado do enlace que a ponte relata (`conectada`, `reconectando` ou `desligada`, "
        "esta última quando não há relato nos últimos 15 s). Sem nenhuma linha desde que a "
        "API subiu, os campos do sinal vêm nulos. O robô parado não envia nada, então o "
        "silêncio longo não prova desconexão."
    ),
)
def estado_robo(request: Request) -> EstadoRobo:
    monitor = request.app.state.monitor_conexao
    ponte, porta = request.app.state.estado_ponte.situacao()
    pedida = request.app.state.estado_ponte.reconexao_pedida
    ultima = monitor.ultima_mensagem
    if ultima is None:
        return EstadoRobo(ponte=ponte, porta=porta, reconexao_pedida=pedida)
    return EstadoRobo(
        ultimo_sinal_em=ultima,
        segundos_sem_sinal=max(0.0, (monitor.agora() - ultima).total_seconds()),
        ponte=ponte,
        porta=porta,
        reconexao_pedida=pedida,
    )


@router.post(
    "/ponte",
    response_model=RespostaRelatoPonte,
    summary="Relato periódico da ponte sobre o enlace",
    description=(
        "A ponte informa a cada poucos segundos se a porta serial está aberta ou sendo "
        "reaberta. A resposta traz `reconectar` verdadeiro uma única vez depois que a tela "
        "pediu a reconexão."
    ),
)
def relatar_ponte(relato: RelatoPonte, request: Request) -> RespostaRelatoPonte:
    reconectar = request.app.state.estado_ponte.registrar(relato.estado, relato.porta)
    return RespostaRelatoPonte(reconectar=reconectar)


@router.post(
    "/reconectar",
    response_model=PedidoReconexao,
    status_code=status.HTTP_202_ACCEPTED,
    summary="Pedir que a ponte reabra a porta do robô",
    description=(
        "Registra o pedido; a ponte o recebe no próximo relato e reabre a porta serial. "
        "Recusa com 409 quando a ponte não relata há mais de 15 s."
    ),
    responses={status.HTTP_409_CONFLICT: {"description": "A ponte não está ligada."}},
)
def pedir_reconexao(request: Request) -> PedidoReconexao:
    if not request.app.state.estado_ponte.pedir_reconexao():
        raise HTTPException(status.HTTP_409_CONFLICT, detail="A ponte não está ligada.")
    return PedidoReconexao(aceito=True)
