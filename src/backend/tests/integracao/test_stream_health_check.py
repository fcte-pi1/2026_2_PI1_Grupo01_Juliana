"""Stream do health-check, independente da execução."""

import json
import threading
import time

from app.services.evento_health_check import CANAL_HEALTH_CHECK, INTERVALO_RELOGIO_S
from app.services.fluxo_sse import fluxo_eventos
from app.services.publicador_sse import EventoSSE
from contrato_utils import validar_contra
from sse_test_utils import parse_sse_frames
from tests.integracao.apoio_health_check import gravar_health_check

RECEBIDO_EM = "2026-10-24T03:00:00+00:00"
HC_ITEM = json.dumps(
    {
        "v": 1,
        "boot": 7,
        "seq": 0,
        "t_ms": 1000,
        "tipo": "hc_item",
        "componente": "bateria",
        "aprovado": True,
        "valor": 7840,
    },
    separators=(",", ":"),
)


def test_relogio_da_stream_e_de_um_segundo():
    assert INTERVALO_RELOGIO_S == 1.0


def test_stream_abre_com_o_get_e_o_relogio_de_um_segundo(client, db_session, monkeypatch):
    gravar_health_check(db_session)
    visto = {}
    publicador = client.app.state.publicador_sse

    def espiao(assinatura, reenvio, *, encerrar_apos_reenvio=False, intervalo_keepalive=15.0):
        visto["intervalo"] = intervalo_keepalive
        return fluxo_eventos(
            assinatura,
            [*reenvio, EventoSSE(tipo="sucesso", dados="{}", encerra=True)],
            encerrar_apos_reenvio=encerrar_apos_reenvio,
            intervalo_keepalive=intervalo_keepalive,
        )

    monkeypatch.setattr("app.routers.health_check.fluxo_eventos", espiao)

    def fechar_se_continuar_aberta():
        limite = time.perf_counter() + 5
        while publicador.assinantes(CANAL_HEALTH_CHECK) == 0 and time.perf_counter() < limite:
            time.sleep(0.01)
        publicador.publicar(
            CANAL_HEALTH_CHECK, [EventoSSE(tipo="sucesso", dados="{}", encerra=True)]
        )

    thread = threading.Thread(target=fechar_se_continuar_aberta, daemon=True)
    thread.start()
    resposta = client.get("/health-check/stream")
    thread.join(timeout=5)

    assert resposta.status_code == 200
    assert resposta.headers["content-type"].startswith("text/event-stream")
    assert visto["intervalo"] == INTERVALO_RELOGIO_S
    frames = parse_sse_frames(resposta.text)
    assert frames[0]["event"] == "health_check"
    assert frames[0]["data"]["aprovado"] is True
    assert frames[0]["data"]["tipo_dip"] == "4x4"
    validar_contra("EventoHealthCheckAtual", frames[0]["data"])
    assert client.app.state.publicador_sse.assinantes(CANAL_HEALTH_CHECK) == 0


def test_publicacao_chega_na_stream_sem_execucao(client):
    publicador = client.app.state.publicador_sse
    evento = EventoSSE(
        tipo="health_check",
        dados=(
            '{"tipo":"health_check","aprovado":null,"tipo_dip":null,'
            '"iniciado_em":"2026-10-24T03:00:00Z","concluido_em":null,'
            '"valido_ate":null,"itens":[]}'
        ),
    )
    terminal = EventoSSE(tipo="sucesso", dados="{}", encerra=True)

    def publicar_quando_assinar():
        limite = time.perf_counter() + 5
        while publicador.assinantes(CANAL_HEALTH_CHECK) == 0 and time.perf_counter() < limite:
            time.sleep(0.005)
        publicador.publicar(CANAL_HEALTH_CHECK, [evento, terminal])

    thread = threading.Thread(target=publicar_quando_assinar, daemon=True)
    thread.start()
    resposta = client.get("/health-check/stream")
    thread.join(timeout=5)

    frames = parse_sse_frames(resposta.text)
    assert [frame["event"] for frame in frames] == ["health_check", "sucesso"]
    assert frames[0]["data"]["aprovado"] is None


def test_hc_item_sem_tentativa_entra_na_stream(client, monkeypatch):
    chamadas = []
    publicador = client.app.state.publicador_sse
    original = publicador.publicar

    def publicar(canal, eventos):
        chamadas.append((canal, list(eventos)))
        original(canal, eventos)

    monkeypatch.setattr(publicador, "publicar", publicar)

    resposta = client.post("/telemetria", json={"linha": HC_ITEM, "recebido_em": RECEBIDO_EM})

    assert resposta.status_code == 200
    canal, eventos = chamadas[0]
    assert canal == CANAL_HEALTH_CHECK
    dados = json.loads(eventos[0].dados)
    assert dados["aprovado"] is None
    assert dados["itens"][0]["componente"] == "bateria"
    assert dados["itens"][0]["aprovado"] is True
    validar_contra("EventoHealthCheckAtual", dados)
