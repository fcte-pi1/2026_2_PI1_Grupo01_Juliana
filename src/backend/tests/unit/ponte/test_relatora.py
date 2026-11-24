import json
from datetime import datetime, timedelta, timezone

import httpx
from ponte.auditoria import Auditoria
from ponte.relatora import Relatora

INICIO = datetime(2026, 11, 21, 9, 0, 0, tzinfo=timezone(timedelta(hours=-3)))


def montar(tmp_path, respostas, aberta=True):
    respostas = list(respostas)
    pedidos = []
    reconexoes = []

    def responder(pedido):
        pedidos.append(json.loads(pedido.content))
        resposta = respostas.pop(0)
        if isinstance(resposta, BaseException):
            raise resposta
        status, corpo = resposta
        return httpx.Response(status, json=corpo)

    cliente = httpx.Client(
        base_url="http://api", transport=httpx.MockTransport(responder), timeout=2.0
    )
    auditoria = Auditoria(tmp_path, relogio=lambda: INICIO)
    relatora = Relatora(
        cliente,
        "/dev/falsa",
        lambda: aberta,
        lambda: reconexoes.append(1),
        auditoria,
        dormir=lambda _s: None,
    )
    return relatora, pedidos, reconexoes, auditoria


def test_relata_conectada_quando_a_porta_esta_aberta(tmp_path):
    relatora, pedidos, reconexoes, _ = montar(tmp_path, [(200, {"reconectar": False})])
    relatora.relatar()
    assert pedidos == [{"estado": "conectada", "porta": "/dev/falsa"}]
    assert reconexoes == []


def test_relata_reconectando_quando_a_porta_esta_fechada(tmp_path):
    relatora, pedidos, _, _ = montar(tmp_path, [(200, {"reconectar": False})], aberta=False)
    relatora.relatar()
    assert pedidos == [{"estado": "reconectando", "porta": "/dev/falsa"}]


def test_reabre_a_porta_quando_a_tela_pediu(tmp_path):
    relatora, _, reconexoes, _ = montar(tmp_path, [(200, {"reconectar": True})])
    relatora.relatar()
    assert reconexoes == [1]


def test_api_fora_do_ar_nao_derruba_e_registra_uma_vez(tmp_path):
    erro = httpx.ConnectError("sem rota")
    relatora, _, reconexoes, auditoria = montar(
        tmp_path, [erro, erro, (200, {"reconectar": False})]
    )
    relatora.relatar()
    relatora.relatar()
    relatora.relatar()
    auditoria.fechar()
    linhas = [linha for linha in auditoria.caminho.read_text().splitlines() if "relato" in linha]
    assert len(linhas) == 1
    assert reconexoes == []
