"""Stream SSE da execução: reenvio, Last-Event-ID e publicação (BACK-07).

O TestClient desta versão acumula o corpo inteiro antes de devolver a resposta,
então todo teste aqui abre um stream que termina por conta própria: ou a execução
já está encerrada, e o fluxo fecha depois do reenvio, ou alguém publica um evento
terminal. Entrega ao vivo contínua e keep-alive ficam nos testes de `fluxo_sse`.
"""

import json
import threading
import time
from datetime import UTC, datetime, timedelta
from uuid import UUID

import pytest
from app.models import Tentativa
from app.repositories.execucoes import ExecucaoRepository
from app.repositories.telemetria import TelemetriaRepository
from app.services.publicador_sse import EventoSSE
from sse_test_utils import count_keep_alives, parse_sse_frames
from tests.integracao.apoio_health_check import gravar_health_check

EXECUCAO_INEXISTENTE = "7c9e6679-7425-40de-944b-e07fc1f90ae7"
CELULAS = [(0, 0), (0, 1), (0, 2), (1, 2), (2, 2)]
RECEBIDO_EM = "2026-10-08T12:00:00+00:00"
LINHA_PASSO = json.dumps(
    {"v": 1, "boot": 7, "seq": 1, "t_ms": 10, "tipo": "passo", "x": 0, "y": 1, "paredes": 5},
    separators=(",", ":"),
)


class Espia:
    """Publicador de mentira: guarda o que a ingestão publicou e quando."""

    def __init__(self) -> None:
        self.publicados: list[tuple[float, str, list[EventoSSE]]] = []

    def publicar(self, execucao_id: str, eventos: list[EventoSSE]) -> None:
        self.publicados.append((time.perf_counter(), execucao_id, eventos))

    @property
    def eventos(self) -> list[EventoSSE]:
        return [evento for _, _, lote in self.publicados for evento in lote]


class EspiaPersistencia(Espia):
    def __init__(self, db_session, tentativa_id) -> None:
        super().__init__()
        self.db_session = db_session
        self.tentativa_id = UUID(str(tentativa_id))

    def publicar(self, execucao_id: str, eventos: list[EventoSSE]) -> None:
        self.db_session.expire_all()
        assert self.db_session.get(Tentativa, self.tentativa_id).status == "failed"
        super().publicar(execucao_id, eventos)


def _abrir(client, execucao_id, **kwargs) -> str:
    """Corpo inteiro do stream. Só para fluxo que encerra sozinho."""
    resposta = client.get(f"/execucoes/{execucao_id}/stream", **kwargs)
    assert resposta.status_code == 200
    return resposta.text


def _enviar(client, linha: str):
    return client.post("/telemetria", json={"linha": linha, "recebido_em": RECEBIDO_EM})


def test_stream_de_execucao_inexistente_devolve_404(client):
    resposta = client.get(f"/execucoes/{EXECUCAO_INEXISTENTE}/stream")

    assert resposta.status_code == 404
    assert resposta.json()["detail"] == "Execução não encontrada."


def test_stream_responde_event_stream_e_abre_com_keep_alive(client, fabrica):
    execucao = fabrica.execucao()
    fabrica.tentativa(execucao)

    resposta = client.get(f"/execucoes/{execucao.execucao_id}/stream")

    assert resposta.status_code == 200
    assert resposta.headers["content-type"].startswith("text/event-stream")
    assert resposta.headers["cache-control"] == "no-cache, no-transform"
    assert count_keep_alives(resposta.text) >= 1


def test_quem_abre_no_meio_da_corrida_recebe_o_trajeto_inteiro(client, fabrica):
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao)
    passos = fabrica.passos(tentativa, CELULAS)

    frames = parse_sse_frames(_abrir(client, execucao.execucao_id))

    assert [f["event"] for f in frames] == ["passo"] * 5
    assert [f["id"] for f in frames] == [p.passo_id for p in passos]
    assert [f["data"]["seq"] for f in frames] == [1, 2, 3, 4, 5]


def test_last_event_id_reenvia_so_os_seguintes_em_ordem_e_sem_duplicar(client, fabrica):
    """Caiu no passo 40 e voltou no 45: recebe 41 a 45, em ordem e uma vez só."""
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao)
    passos = fabrica.passos(tentativa, CELULAS)
    ja_aplicado = passos[1].passo_id

    frames = parse_sse_frames(
        _abrir(client, execucao.execucao_id, headers={"Last-Event-ID": str(ja_aplicado)})
    )

    ids = [f["id"] for f in frames]
    assert ids == [p.passo_id for p in passos[2:]]
    assert ids == sorted(ids)
    assert len(set(ids)) == len(ids)


@pytest.mark.parametrize("valor", ["abc", "", "-1", "0", "9,5"])
def test_last_event_id_invalido_vale_zero_e_reenvia_tudo(client, fabrica, valor):
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao)
    fabrica.passos(tentativa, CELULAS)

    frames = parse_sse_frames(
        _abrir(client, execucao.execucao_id, headers={"Last-Event-ID": valor})
    )

    assert len(frames) == 5


def test_execucao_encerrada_reenvia_o_trajeto_e_fecha_o_fluxo(client, fabrica):
    execucao = fabrica.execucao(status="cancelada")
    tentativa = fabrica.tentativa(execucao, status="failed")
    fabrica.passos(tentativa, CELULAS[:3])

    # se o fluxo não fechasse sozinho, esta chamada não retornaria
    frames = parse_sse_frames(_abrir(client, execucao.execucao_id))

    assert len(frames) == 3


def test_trajeto_atravessa_as_tentativas_da_execucao(client, fabrica):
    """O reenvio é por execução: as três tentativas são um só trajeto (RF09)."""
    execucao = fabrica.execucao(tentativas_usadas=2)
    primeira = fabrica.tentativa(execucao, attempt_index=1, status="failed")
    segunda = fabrica.tentativa(execucao, attempt_index=2, status="success")
    fabrica.passos(primeira, CELULAS[:3], seq_inicial=1)
    fabrica.passos(segunda, CELULAS[2:], seq_inicial=4)

    frames = parse_sse_frames(_abrir(client, execucao.execucao_id))

    assert [f["data"]["seq"] for f in frames] == [1, 2, 3, 4, 5, 6]
    assert frames[3]["data"]["retomada"] is True


def test_evento_publicado_de_outra_thread_chega_depois_do_reenvio(client, fabrica):
    """Caminho real: quem publica é a thread do threadpool da ingestão."""
    execucao = fabrica.execucao(status="em_andamento")
    tentativa = fabrica.tentativa(execucao, status="running")
    passos = fabrica.passos(tentativa, CELULAS[:2])
    publicador = client.app.state.publicador_sse
    execucao_id = str(execucao.execucao_id)
    ao_vivo = EventoSSE(tipo="passo", dados='{"tipo":"passo","passo_id":9999}', id=9999)
    # encerra o fluxo, senão o stream ficaria aberto para sempre
    terminal = EventoSSE(tipo="sucesso", dados='{"tipo":"sucesso"}', encerra=True)

    def publicar_quando_o_cliente_assinar():
        limite = time.perf_counter() + 5
        while publicador.assinantes(execucao_id) == 0 and time.perf_counter() < limite:
            time.sleep(0.005)
        publicador.publicar(execucao_id, [ao_vivo, terminal])

    thread = threading.Thread(target=publicar_quando_o_cliente_assinar, daemon=True)
    thread.start()
    frames = parse_sse_frames(_abrir(client, execucao.execucao_id))
    thread.join(timeout=5)

    assert [f["event"] for f in frames] == ["passo", "passo", "passo", "sucesso"]
    assert [f["id"] for f in frames[:3]] == [passos[0].passo_id, passos[1].passo_id, 9999]


def test_execucao_que_encerra_logo_apos_a_leitura_do_status_ainda_fecha_o_fluxo(
    client, fabrica, monkeypatch
):
    execucao = fabrica.execucao(status="em_andamento")
    fabrica.tentativa(execucao, status="running")
    execucao_id = str(execucao.execucao_id)
    publicador = client.app.state.publicador_sse
    terminal = EventoSSE(tipo="sucesso", dados='{"tipo":"sucesso"}', encerra=True)
    status_de = ExecucaoRepository.status_de

    def status_e_depois_encerra(self, id_execucao):
        status_lido = status_de(self, id_execucao)
        # a execução termina depois de o status ser lido como em_andamento
        publicador.publicar(execucao_id, [terminal])
        return status_lido

    monkeypatch.setattr(ExecucaoRepository, "status_de", status_e_depois_encerra)

    frames = parse_sse_frames(_abrir(client, execucao_id))

    assert frames[-1]["event"] == "sucesso"


def test_assinatura_sai_do_registro_quando_o_fluxo_fecha(client, fabrica):
    execucao = fabrica.execucao()
    fabrica.tentativa(execucao)
    publicador = client.app.state.publicador_sse

    _abrir(client, execucao.execucao_id)

    assert publicador.assinantes(str(execucao.execucao_id)) == 0
    assert publicador.execucoes_ativas() == 0


def test_evento_sai_em_menos_de_um_segundo_do_registro_no_banco(client, fabrica, monkeypatch):
    """Fatia de backend do RNF-B04: do COMMIT até o evento ser entregue."""
    execucao = fabrica.execucao(status="em_andamento")
    fabrica.tentativa(execucao, status="running")
    espia = Espia()
    monkeypatch.setattr(client.app.state.ingestao_telemetria, "publicador", espia)

    comeco = time.perf_counter()
    resposta = _enviar(client, LINHA_PASSO)

    assert resposta.status_code == 200
    assert len(espia.publicados) == 1
    instante, execucao_id, eventos = espia.publicados[0]
    assert instante - comeco < 1.0
    assert execucao_id == str(execucao.execucao_id)
    assert [evento.tipo for evento in eventos] == ["passo"]
    assert eventos[0].id is not None


def test_releitura_da_mesma_celula_republica_o_passo_com_a_parede_nova(
    client, fabrica, monkeypatch
):
    """Mesmo passo_id de propósito: a parede é nova e o desenho precisa dela."""
    execucao = fabrica.execucao(status="em_andamento")
    fabrica.tentativa(execucao, status="running")
    espia = Espia()
    monkeypatch.setattr(client.app.state.ingestao_telemetria, "publicador", espia)

    assert _enviar(client, LINHA_PASSO).status_code == 200
    outra_parede = LINHA_PASSO.replace('"seq":1', '"seq":2').replace('"paredes":5', '"paredes":9')
    assert _enviar(client, outra_parede).status_code == 200

    eventos = espia.eventos
    assert [evento.id for evento in eventos] == [eventos[0].id, eventos[0].id]
    assert json.loads(eventos[0].dados)["paredes_mask"] == 5
    assert json.loads(eventos[1].dados)["paredes_mask"] == 9


def test_telemetria_publicada_leva_mv_porcentagem_e_rumo(client, fabrica, monkeypatch):
    execucao = fabrica.execucao(status="em_andamento")
    fabrica.tentativa(execucao, status="running")
    espia = Espia()
    monkeypatch.setattr(client.app.state.ingestao_telemetria, "publicador", espia)
    linha = json.dumps(
        {
            "v": 1,
            "boot": 7,
            "seq": 42,
            "t_ms": 11250,
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 2,
            "rumo": "N",
            "bat_mv": 7810,
            "vel_mm_s": 210,
            "eixo_longo": None,
        },
        separators=(",", ":"),
    )

    assert _enviar(client, linha).status_code == 200

    evento = espia.eventos[0]
    assert evento.tipo == "telemetria"
    # a amostra de telemetria não entra no reenvio, então não tem linha id
    assert evento.id is None
    dados = json.loads(evento.dados)
    assert dados["bateria"] == 7810
    assert dados["bateria_pct"] == 56.0
    assert dados["rumo"] == "N"


def test_falha_na_publicacao_nao_desfaz_a_gravacao(client, fabrica, db_session, monkeypatch):
    """Um navegador problemático não pode custar uma linha de telemetria (RNF-B07)."""
    execucao = fabrica.execucao(status="em_andamento")
    fabrica.tentativa(execucao, status="running")

    class Quebrado:
        def publicar(self, execucao_id, eventos):
            raise RuntimeError("publicador com defeito")

    monkeypatch.setattr(client.app.state.ingestao_telemetria, "publicador", Quebrado())

    resposta = _enviar(client, LINHA_PASSO)

    assert resposta.status_code == 200
    assert TelemetriaRepository(db_session).ultimo_passo(execucao.execucao_id) is not None


def test_link_lost_na_ingestao_publica_falha_retomavel_apos_commit(
    client, fabrica, db_session, monkeypatch
):
    inicio = datetime.now(UTC) - timedelta(seconds=20)
    execucao = fabrica.execucao(status="em_andamento", iniciada_em=inicio)
    tentativa = fabrica.tentativa(execucao, status="running")
    db_session.commit()
    client.app.state.monitor_conexao._ultima_mensagem = datetime.now(UTC) - timedelta(seconds=11)
    espia = EspiaPersistencia(db_session, tentativa.tentativa_id)
    monkeypatch.setattr(client.app.state.ingestao_telemetria, "publicador", espia)

    assert _enviar(client, LINHA_PASSO).status_code == 200

    status, evento = espia.eventos
    assert status.tipo == "status"
    assert json.loads(status.dados)["status_execucao"] == "em_andamento"
    assert evento.tipo == "falha"
    assert evento.encerra is False
    assert json.loads(evento.dados)["motivo"] == "link_lost"


@pytest.fixture
def health_check(db_session):
    return gravar_health_check(db_session, "4x4")


def test_encerrar_tentativa_publica_falha_retomavel_apos_commit(
    client, db_session, monkeypatch, health_check
):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa_id = criado["tentativa_id"]
    espia = EspiaPersistencia(db_session, tentativa_id)
    monkeypatch.setattr(client.app.state, "publicador_sse", espia)

    resposta = client.post(
        f"/execucoes/{criado['execucao_id']}/encerrar-tentativa",
        json={"motivo": "collision"},
    )

    assert resposta.status_code == 200
    [evento] = espia.eventos
    assert evento.tipo == "falha"
    assert evento.encerra is False
    assert json.loads(evento.dados)["origem"] == "encerrado_operador"


def test_encerrar_execucao_publica_falha_que_fecha_o_stream(
    client, db_session, monkeypatch, health_check
):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    espia = EspiaPersistencia(db_session, criado["tentativa_id"])
    monkeypatch.setattr(client.app.state, "publicador_sse", espia)

    resposta = client.post(f"/execucoes/{criado['execucao_id']}/encerrar")

    assert resposta.status_code == 200
    [evento] = espia.eventos
    assert evento.tipo == "falha"
    assert evento.encerra is True
    assert json.loads(evento.dados)["status_execucao"] == "cancelada"
    assert json.loads(evento.dados)["motivo"] == "encerrado_operador"
