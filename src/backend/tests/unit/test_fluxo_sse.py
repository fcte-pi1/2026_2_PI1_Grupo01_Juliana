"""Frames, reenvio, dedupe e keep-alive do fluxo de um cliente (BACK-07)."""

from app.services.fluxo_sse import COMENTARIO_KEEPALIVE, fluxo_eventos, montar_frame
from app.services.publicador_sse import EventoSSE, PublicadorSSE

EXECUCAO = "7c9e6679-7425-40de-944b-e07fc1f90ae7"


def _passo(n: int) -> EventoSSE:
    return EventoSSE(tipo="passo", dados=f'{{"tipo":"passo","passo_id":{n}}}', id=n)


def _ids(pedacos: list[str]) -> list[int]:
    return [int(p.splitlines()[0].removeprefix("id: ")) for p in pedacos if p.startswith("id:")]


def test_frame_do_passo_tem_id_event_e_data():
    assert montar_frame(_passo(41)) == (
        'id: 41\nevent: passo\ndata: {"tipo":"passo","passo_id":41}\n\n'
    )


def test_frame_sem_id_nao_traz_a_linha_id():
    frame = montar_frame(EventoSSE(tipo="telemetria", dados='{"tipo":"telemetria"}'))

    assert frame == 'event: telemetria\ndata: {"tipo":"telemetria"}\n\n'


async def test_abre_com_keep_alive_reenvia_em_ordem_e_fecha_execucao_encerrada():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)

    pedacos = [
        pedaco
        async for pedaco in fluxo_eventos(
            assinatura, [_passo(1), _passo(2)], encerrar_apos_reenvio=True
        )
    ]

    assert pedacos[0] == COMENTARIO_KEEPALIVE
    assert _ids(pedacos) == [1, 2]
    # o finally saiu do registro, sem depender do cliente fechar a conexão
    assert publicador.execucoes_ativas() == 0


async def test_passo_ja_reenviado_nao_repete_mas_a_atualizacao_de_parede_passa():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    # chegaram na fila enquanto o reenvio era consultado no banco
    assinatura.fila.put_nowait(_passo(2))  # cópia do que já foi: descartada
    assinatura.fila.put_nowait(_passo(3))  # passo novo: passa
    assinatura.fila.put_nowait(_passo(2))  # releitura da parede do mesmo passo: passa
    assinatura.fila.put_nowait(None)

    pedacos = [pedaco async for pedaco in fluxo_eventos(assinatura, [_passo(1), _passo(2)])]

    assert _ids(pedacos) == [1, 2, 3, 2]


async def test_quem_abre_no_meio_recebe_o_reenvio_inteiro_antes_do_ao_vivo():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    assinatura.fila.put_nowait(_passo(6))
    assinatura.fila.put_nowait(None)

    pedacos = [
        pedaco
        async for pedaco in fluxo_eventos(assinatura, [_passo(n) for n in range(1, 6)])
    ]

    assert _ids(pedacos) == [1, 2, 3, 4, 5, 6]


async def test_keep_alive_sai_sozinho_quando_nao_ha_evento():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    gerador = fluxo_eventos(assinatura, [], intervalo_keepalive=0.01)

    pedacos = []
    async for pedaco in gerador:
        pedacos.append(pedaco)
        if len(pedacos) == 3:
            break
    await gerador.aclose()

    assert pedacos == [COMENTARIO_KEEPALIVE] * 3
    assert publicador.execucoes_ativas() == 0


async def test_evento_terminal_encerra_o_fluxo():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    assinatura.fila.put_nowait(EventoSSE(tipo="sucesso", dados='{"tipo":"sucesso"}', encerra=True))
    assinatura.fila.put_nowait(_passo(99))  # não deve ser entregue depois do terminal

    pedacos = [pedaco async for pedaco in fluxo_eventos(assinatura, [])]

    assert pedacos[-1].startswith("event: sucesso")
    assert _ids(pedacos) == []


async def test_sentinela_de_fila_estourada_encerra_o_fluxo():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    assinatura.fila.put_nowait(_passo(1))
    assinatura.fila.put_nowait(None)

    pedacos = [pedaco async for pedaco in fluxo_eventos(assinatura, [])]

    assert _ids(pedacos) == [1]
    assert publicador.execucoes_ativas() == 0
