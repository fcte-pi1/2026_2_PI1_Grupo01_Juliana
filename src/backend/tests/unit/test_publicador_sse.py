"""Fila por cliente e entrega entre threads do publicador SSE (BACK-07)."""

import asyncio

from app.services import publicador_sse
from app.services.publicador_sse import EventoSSE, PublicadorSSE

EXECUCAO = "7c9e6679-7425-40de-944b-e07fc1f90ae7"
OUTRA = "123e4567-e89b-12d3-a456-426614174000"


def _passo(n: int) -> EventoSSE:
    return EventoSSE(tipo="passo", dados=f'{{"passo_id":{n}}}', id=n)


async def test_desassinar_apaga_a_chave_da_execucao():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    assert publicador.assinantes(EXECUCAO) == 1

    assinatura.fechar()

    assert publicador.assinantes(EXECUCAO) == 0
    # a chave sai do dicionário: sem isso ele cresceria a cada execução vista
    assert publicador.execucoes_ativas() == 0


async def test_publica_para_todos_os_clientes_da_mesma_execucao():
    publicador = PublicadorSSE()
    primeiro = publicador.assinar(EXECUCAO)
    segundo = publicador.assinar(EXECUCAO)
    de_outra_execucao = publicador.assinar(OUTRA)

    publicador.publicar(EXECUCAO, [_passo(1)])
    # a entrega é sempre agendada no loop, mesmo publicando de dentro dele
    await asyncio.sleep(0)

    assert primeiro.fila.get_nowait().id == 1
    assert segundo.fila.get_nowait().id == 1
    assert de_outra_execucao.fila.empty()


async def test_publicar_sem_assinante_nao_levanta():
    publicador = PublicadorSSE()

    publicador.publicar(EXECUCAO, [_passo(1)])

    assert publicador.execucoes_ativas() == 0


async def test_publicacao_de_outra_thread_chega_na_fila():
    """É o caminho real: a ingestão roda no threadpool, o cliente no event loop."""
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)

    await asyncio.to_thread(publicador.publicar, EXECUCAO, [_passo(7)])

    evento = await asyncio.wait_for(assinatura.fila.get(), 1)
    assert evento.id == 7


async def test_ordem_da_fila_e_a_ordem_da_publicacao():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)

    for n in (1, 2, 3):
        await asyncio.to_thread(publicador.publicar, EXECUCAO, [_passo(n)])

    assert [assinatura.fila.get_nowait().id for _ in range(3)] == [1, 2, 3]


async def test_fila_cheia_encerra_o_cliente_para_ele_reconectar(monkeypatch):
    monkeypatch.setattr(publicador_sse, "TAMANHO_FILA", 3)
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)

    publicador.publicar(EXECUCAO, [_passo(n) for n in range(1, 6)])
    await asyncio.sleep(0)  # deixa os callbacks agendados no loop rodarem

    assert assinatura.encerrada
    assert publicador.assinantes(EXECUCAO) == 0
    itens = []
    while not assinatura.fila.empty():
        itens.append(assinatura.fila.get_nowait())
    # a sentinela entra mesmo com a fila cheia: é ela que encerra o fluxo
    assert itens[-1] is None


async def test_cliente_encerrado_nao_recebe_mais_nada():
    publicador = PublicadorSSE()
    assinatura = publicador.assinar(EXECUCAO)
    assinatura.fechar()

    publicador.publicar(EXECUCAO, [_passo(1)])
    await asyncio.sleep(0)

    assert assinatura.fila.empty()


def test_loop_fechado_nao_levanta_e_desassina():
    """Cliente cujo loop já morreu: publicar não pode quebrar a ingestão."""
    publicador = PublicadorSSE()

    async def assinar():
        publicador.assinar(EXECUCAO)

    asyncio.run(assinar())
    assert publicador.assinantes(EXECUCAO) == 1

    publicador.publicar(EXECUCAO, [_passo(1)])

    assert publicador.assinantes(EXECUCAO) == 0
