"""Frames do stream validados contra os schemas Evento* do openapi.yaml (BACK-07)."""

import json

from app.services.fabrica_eventos import (
    FABRICAS,
    OrigemFalha,
    OrigemHealthCheck,
    OrigemStatus,
    OrigemSucesso,
    OrigemTelemetria,
)
from contrato.telemetria import ler_linha
from contrato_utils import validar_contra
from sse_test_utils import parse_sse_frames

CELULAS = [(0, 0), (0, 1), (0, 2)]
LINHA_TEL = (
    '{"v":1,"boot":7,"seq":43,"t_ms":11400,"tipo":"tel","estado":"running",'
    '"x":0,"y":2,"rumo":"N","bat_mv":7810,"vel_mm_s":210,"eixo_longo":null}'
)


def test_frames_do_stream_seguem_o_contrato(client, fabrica):
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao)
    # paredes_mask nulo no banco: o contrato exige a máscara entre 0 e 15
    fabrica.passos(tentativa, CELULAS, paredes_mask=None)

    resposta = client.get(f"/execucoes/{execucao.execucao_id}/stream")
    frames = parse_sse_frames(resposta.text)

    assert len(frames) == 3
    for frame in frames:
        # a linha event tem de repetir o tipo do JSON, como o openapi manda
        assert frame["event"] == frame["data"]["tipo"]
        validar_contra("EventoSSE", frame["data"])


def test_so_o_passo_traz_a_linha_id(client, fabrica):
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao)
    fabrica.passos(tentativa, CELULAS)

    frames = parse_sse_frames(client.get(f"/execucoes/{execucao.execucao_id}/stream").text)

    for frame in frames:
        assert ("id" in frame) == (frame["event"] == "passo")
        assert frame["id"] == frame["data"]["passo_id"]


def test_os_seis_eventos_do_contrato_tem_payload_valido(fabrica):
    """Inclui os três que o BACK-03 vai publicar, para o contrato já cobri-los."""
    execucao = fabrica.execucao(status="em_andamento", tentativas_usadas=1)
    tentativa = fabrica.tentativa(execucao, status="running")
    passo = fabrica.passos(tentativa, CELULAS)[0]
    item = fabrica.health_check(tentativa).itens[0]
    falha = fabrica.falha(tentativa)

    origens = [
        passo,
        OrigemTelemetria(tel=ler_linha(LINHA_TEL), ordem=42, enviado_em=tentativa.iniciada_em),
        OrigemHealthCheck(item=item, tentativa_id=tentativa.tentativa_id),
        OrigemStatus(tentativa=tentativa, status_execucao="em_andamento"),
        OrigemFalha(falha=falha, status_execucao="cancelada"),
        OrigemSucesso(tentativa=tentativa, x=3, y=3, status_execucao="concluida"),
    ]

    tipos = []
    for origem in origens:
        evento = FABRICAS.criar(origem)
        dados = json.loads(evento.dados)
        assert evento.tipo == dados["tipo"]
        validar_contra("EventoSSE", dados)
        tipos.append(evento.tipo)

    assert tipos == ["passo", "telemetria", "health_check", "status", "falha", "sucesso"]
