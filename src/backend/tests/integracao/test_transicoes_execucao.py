"""Transições BACK-03 (T/E/R) — oráculo transicoes-execucao.md."""

from __future__ import annotations

import uuid
from datetime import UTC, datetime, timedelta

import pytest
from app.main import app
from app.models import ExecucaoLogica, LeituraTelemetria, Tentativa, TentativaRejeitada
from app.repositories.repositorio import Repositorio
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from app.services.vigilancia_monitores import gravar_se_cancelou
from contrato.telemetria import Falha, Sucesso, Tel, escrever_linha
from fastapi.testclient import TestClient
from sqlalchemy import select
from sqlalchemy.orm import Session
from tests.integracao.apoio_health_check import gravar_health_check


@pytest.fixture
def gerenciador() -> GerenciadorExecucoes:
    return app.state.gerenciador


@pytest.fixture(autouse=True)
def health_check_aprovado(db_session: Session):
    """Pré-requisito de toda tentativa: um health-check 4x4 aprovado há pouco."""
    return gravar_health_check(db_session, "4x4")


def _falha_auto(motivo: str = "stuck", x: int = 1, y: int = 2) -> Falha:
    return Falha(
        v=1,
        boot=1,
        seq=2,
        t_ms=200,
        tipo="falha",
        motivo=motivo,
        origem="automatica",
        x=x,
        y=y,
        componente=None,
    )


def _sucesso(x: int = 3, y: int = 3) -> Sucesso:
    return Sucesso(v=1, boot=1, seq=3, t_ms=300, tipo="sucesso", x=x, y=y)


def _aplicar_perda_link(
    db_session: Session, gerenciador: GerenciadorExecucoes
) -> tuple[ExecucaoLogica, Tentativa]:
    ret = gerenciador.nova_execucao(db_session, "4x4")
    tentativa = db_session.get(Tentativa, ret.tentativa_id)
    execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
    inicio = datetime.now(UTC) - timedelta(seconds=20)
    tentativa.iniciada_em = inicio
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    monitor.registrar_mensagem(datetime.now(UTC) - timedelta(seconds=11))
    gerenciador.verificar_monitores(db_session, monitor)
    return execucao, tentativa


def _aplicar_tempo_excedido(
    db_session: Session, gerenciador: GerenciadorExecucoes
) -> tuple[ExecucaoLogica, Tentativa]:
    ret = gerenciador.nova_execucao(db_session, "4x4")
    tentativa = db_session.get(Tentativa, ret.tentativa_id)
    execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
    execucao.iniciada_em = datetime.now(UTC) - timedelta(seconds=601)
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    gerenciador.verificar_monitores(db_session, monitor)
    return execucao, tentativa


def _falha_web() -> Falha:
    return Falha(
        v=1,
        boot=1,
        seq=4,
        t_ms=400,
        tipo="falha",
        motivo=None,
        origem="web",
        x=0,
        y=0,
        componente=None,
    )


class TestTransicoesTentativa:
    def test_t01_nova_execucao_abre_running(
        self, client: TestClient, db_session: Session, health_check_aprovado
    ):
        r = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
        assert r.status_code == 201
        body = r.json()
        execucao = db_session.get(ExecucaoLogica, uuid.UUID(body["execucao_id"]))
        tentativa = db_session.get(Tentativa, uuid.UUID(body["tentativa_id"]))
        assert execucao.status == "em_andamento"
        assert execucao.tentativas_usadas == 1
        assert tentativa.status == "running"
        assert tentativa.attempt_index == 1
        assert tentativa.tipo_inicio == "nova"
        assert tentativa.hc_id == health_check_aprovado.hc_id
        assert tentativa.tipo_dip == "4x4"
        assert [c.model_dump() for c in app.state.gerenciador.comandos_pendentes()] == [
            {"v": 1, "cmd": "largar"}
        ]

    def test_largada_descarta_pedido_de_health_check_nao_atendido(
        self, client: TestClient, health_check_aprovado
    ):
        assert client.post("/health-check/solicitar").status_code == 202
        assert client.post("/execucoes", json={"tipo_labirinto": "4x4"}).status_code == 201
        assert [c.model_dump() for c in app.state.gerenciador.comandos_pendentes()] == [
            {"v": 1, "cmd": "largar"}
        ]

    def test_t02_retomar_abre_proxima_tentativa(
        self, client: TestClient, db_session: Session, gerenciador
    ):
        criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        execucao_id = criado["execucao_id"]
        tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        db_session.commit()

        r = client.post(f"/execucoes/{execucao_id}/retomar")
        assert r.status_code == 201
        assert r.json()["attempt_index"] == 2
        execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
        assert execucao.tentativas_usadas == 2

    def test_t03_retomar_reusa_health_check_ainda_valido(
        self, db_session: Session, gerenciador, health_check_aprovado
    ):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        ret = gerenciador.retomar(db_session, ret.execucao_id)
        segunda = db_session.get(Tentativa, ret.tentativa_id)
        assert segunda.status == "running"
        assert segunda.hc_id == health_check_aprovado.hc_id

    def test_t04_hc_reprovado_nao_abre_tentativa(self, client: TestClient, db_session: Session):
        gravar_health_check(db_session, "4x4", aprovado=False)
        r = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
        assert r.status_code == 409
        assert r.json()["motivo"] == "health_check_reprovado"
        assert db_session.scalars(select(Tentativa)).all() == []

    def test_t05_sucesso_conclui_execucao(self, client: TestClient, db_session: Session):
        criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        execucao_id = criado["execucao_id"]
        tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
        g: GerenciadorExecucoes = app.state.gerenciador
        g.aplicar_evento(db_session, tentativa, _sucesso())
        db_session.commit()
        execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
        assert tentativa.status == "success"
        assert execucao.status == "concluida"

    def test_t06_falha_trajeto_mantem_em_andamento(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert tentativa.status == "failed"
        assert execucao.status == "em_andamento"

    def test_t07_encerrar_manual(self, client: TestClient, db_session: Session, gerenciador):
        criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        execucao_id = criado["execucao_id"]
        tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
        db_session.commit()

        r = client.post(
            f"/execucoes/{execucao_id}/encerrar-tentativa",
            json={"motivo": "collision", "observacao": "teste"},
        )
        assert r.status_code == 200
        db_session.expire_all()
        tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
        execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
        assert tentativa.status == "failed"
        assert tentativa.falha.origem == "encerrado_operador"
        assert execucao.status == "em_andamento"
        assert [c.model_dump() for c in gerenciador.comandos_pendentes()] == [
            {"v": 1, "cmd": "interromper"}
        ]

    def test_t08_link_lost_cancela_execucao(self, db_session: Session, gerenciador):
        execucao, tentativa = _aplicar_perda_link(db_session, gerenciador)
        assert tentativa.status == "failed"
        assert execucao.status == "cancelada"
        assert tentativa.falha.motivo == "link_lost"

    def test_t09_tempo_excedido_cancela_execucao(self, db_session: Session, gerenciador):
        execucao, tentativa = _aplicar_tempo_excedido(db_session, gerenciador)
        assert tentativa.status == "failed"
        assert execucao.status == "cancelada"
        assert tentativa.falha.motivo == "time_exceeded"

    def test_t10_failed_e_terminal(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert tentativa.status == "failed"

    def test_t11_success_e_terminal(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _sucesso())
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert tentativa.status == "success"

    def test_t12_terceira_falha_cancela_execucao(self, db_session: Session, gerenciador):
        execucao_id = self._execucao_com_duas_falhas(db_session, gerenciador)
        execucao = db_session.get(ExecucaoLogica, execucao_id)
        ret = gerenciador.retomar(db_session, execucao_id)
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert tentativa.attempt_index == 3
        assert tentativa.status == "failed"
        assert execucao.status == "cancelada"

    def test_t13_nova_execucao_cancela_anterior(self, client: TestClient, db_session: Session):
        primeiro = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        antiga_id = uuid.UUID(primeiro["execucao_id"])
        tentativa_antiga = db_session.get(Tentativa, uuid.UUID(primeiro["tentativa_id"]))

        gravar_health_check(db_session, "8x4")
        client.post("/execucoes", json={"tipo_labirinto": "8x4"})
        db_session.expire_all()
        antiga = db_session.get(ExecucaoLogica, antiga_id)
        db_session.refresh(tentativa_antiga)
        assert antiga.status == "cancelada"
        assert tentativa_antiga.status == "failed"

    def _execucao_com_duas_falhas(self, db_session: Session, gerenciador) -> uuid.UUID:
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        ret = gerenciador.retomar(db_session, ret.execucao_id)
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        return ret.execucao_id


class TestTransicoesExecucao:
    def test_e01_nova_sem_anterior(self, client: TestClient):
        assert client.post("/execucoes", json={"tipo_labirinto": "4x4"}).status_code == 201

    def test_e02_nova_cancela_anterior(self, client: TestClient, db_session: Session):
        a = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        gravar_health_check(db_session, "8x4")
        b = client.post("/execucoes", json={"tipo_labirinto": "8x4"}).json()
        assert db_session.get(ExecucaoLogica, uuid.UUID(a["execucao_id"])).status == "cancelada"
        assert db_session.get(ExecucaoLogica, uuid.UUID(b["execucao_id"])).status == "em_andamento"

    def test_e03_sucesso_conclui(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
        gerenciador.aplicar_evento(db_session, tentativa, _sucesso())
        assert execucao.status == "concluida"

    def test_e04_falha_com_retomada(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert execucao.status == "em_andamento"
        assert Repositorio(db_session).buscar_tentativa_aberta_da_execucao(ret.execucao_id) is None

    def test_e05_terceira_falha_cancela(self, db_session: Session, gerenciador):
        helper = TestTransicoesTentativa()
        execucao_id = helper._execucao_com_duas_falhas(db_session, gerenciador)
        execucao = db_session.get(ExecucaoLogica, execucao_id)
        ret = gerenciador.retomar(db_session, execucao_id)
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        assert execucao.status == "cancelada"

    def test_e06_perda_link_cancela(self, db_session: Session, gerenciador):
        execucao, _ = _aplicar_perda_link(db_session, gerenciador)
        assert execucao.status == "cancelada"

    def test_e07_tempo_execucao_cancela(self, db_session: Session, gerenciador):
        execucao, _ = _aplicar_tempo_excedido(db_session, gerenciador)
        assert execucao.status == "cancelada"

    def test_e08_nova_apos_terminal(self, client: TestClient, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
        gerenciador.aplicar_evento(db_session, tentativa, _sucesso())
        db_session.commit()
        assert execucao.status == "concluida"
        r = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
        assert r.status_code == 201
        nova = db_session.get(ExecucaoLogica, uuid.UUID(r.json()["execucao_id"]))
        assert nova.status == "em_andamento"


class TestRecusas:
    def test_r01_tentativa_aberta(self, client: TestClient, db_session: Session):
        criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        execucao_id = criado["execucao_id"]
        r = client.post(f"/execucoes/{execucao_id}/retomar")
        assert r.status_code == 409
        assert r.json()["motivo"] == "tentativa_aberta"
        eid = uuid.UUID(execucao_id)
        rejeicoes = db_session.scalars(
            select(TentativaRejeitada).where(TentativaRejeitada.execucao_id == eid)
        ).all()
        assert len(rejeicoes) == 1

    def test_r02_retomar_execucao_nao_em_andamento(
        self, client: TestClient, db_session: Session, gerenciador
    ):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _sucesso())
        db_session.commit()

        r = client.post(f"/execucoes/{ret.execucao_id}/retomar")
        assert r.status_code == 409
        assert r.json()["motivo"] == "execucao_cancelada"

    def test_r03_limite_3(self, client: TestClient, db_session: Session, gerenciador):
        helper = TestTransicoesTentativa()
        execucao_id = helper._execucao_com_duas_falhas(db_session, gerenciador)
        ret = gerenciador.retomar(db_session, execucao_id)
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        db_session.commit()

        r = client.post(f"/execucoes/{execucao_id}/retomar")
        assert r.status_code == 409
        assert r.json()["motivo"] == "limite_3"

    def test_r04_encerrar_execucao_cancelada(
        self, client: TestClient, db_session: Session, gerenciador
    ):
        helper = TestTransicoesTentativa()
        execucao_id = helper._execucao_com_duas_falhas(db_session, gerenciador)
        ret = gerenciador.retomar(db_session, execucao_id)
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        db_session.commit()
        r = client.post(
            f"/execucoes/{execucao_id}/encerrar-tentativa",
            json={"motivo": "stuck"},
        )
        assert r.status_code == 409
        assert r.json()["detail"] == "Não há tentativa aberta para encerrar."

    def test_r05_encerrar_sem_tentativa_aberta(
        self, client: TestClient, db_session: Session, gerenciador
    ):
        criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
        execucao_id = criado["execucao_id"]
        tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        db_session.commit()
        assert (
            client.post(
                f"/execucoes/{execucao_id}/encerrar-tentativa",
                json={"motivo": "stuck"},
            ).status_code
            == 409
        )

    def test_confirmacao_web_nao_duplica_falha(self, db_session: Session, gerenciador):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.encerrar_tentativa(db_session, ret.execucao_id, "collision", None)
        falhas_antes = tentativa.falha.falha_id
        gerenciador.aplicar_evento(db_session, tentativa, _falha_web())
        assert tentativa.falha.falha_id == falhas_antes
        assert gerenciador.comandos_pendentes() == []


class TestHealthCheckPreRequisito:
    def _nova(self, client: TestClient, tipo: str = "4x4"):
        return client.post("/execucoes", json={"tipo_labirinto": tipo})

    def test_recusa_nao_cancela_a_execucao_anterior(
        self, client: TestClient, db_session: Session, health_check_aprovado
    ):
        anterior = self._nova(client).json()
        health_check_aprovado.concluido_em = datetime.now(UTC) - timedelta(minutes=8)
        db_session.flush()

        r = self._nova(client)

        assert r.status_code == 409
        assert r.json()["motivo"] == "health_check_expirado"
        execucao = db_session.get(ExecucaoLogica, uuid.UUID(anterior["execucao_id"]))
        assert execucao.status == "em_andamento"

    @pytest.mark.parametrize(("idade_s", "status"), [(415, 201), (421, 409)], ids=["415s", "421s"])
    def test_validade_de_7_minutos(
        self, client: TestClient, db_session: Session, health_check_aprovado, idade_s, status
    ):
        health_check_aprovado.concluido_em = datetime.now(UTC) - timedelta(seconds=idade_s)
        db_session.flush()

        r = self._nova(client)

        assert r.status_code == status
        if status == 409:
            assert r.json()["motivo"] == "health_check_expirado"

    def test_dip_divergente(self, client: TestClient):
        r = self._nova(client, "8x4")
        assert r.status_code == 409
        assert r.json()["motivo"] == "dip_divergente"

    def test_reprovado_mais_recente_vence_o_aprovado_antigo(
        self, client: TestClient, db_session: Session
    ):
        gravar_health_check(db_session, "4x4", aprovado=False)
        r = self._nova(client)
        assert r.status_code == 409
        assert r.json()["motivo"] == "health_check_reprovado"

    def test_retomar_com_health_check_expirado_grava_recusa(
        self, client: TestClient, db_session: Session, gerenciador, health_check_aprovado
    ):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        health_check_aprovado.concluido_em = datetime.now(UTC) - timedelta(minutes=8)
        db_session.commit()

        r = client.post(f"/execucoes/{ret.execucao_id}/retomar")

        assert r.status_code == 409
        assert r.json()["motivo"] == "health_check_expirado"
        [rejeicao] = db_session.scalars(
            select(TentativaRejeitada).where(TentativaRejeitada.execucao_id == ret.execucao_id)
        ).all()
        assert rejeicao.motivo == "health_check_expirado"
        execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
        assert execucao.tentativas_usadas == 1

    def test_retomar_com_novo_health_check_usa_o_novo(
        self, db_session: Session, gerenciador, health_check_aprovado
    ):
        ret = gerenciador.nova_execucao(db_session, "4x4")
        tentativa = db_session.get(Tentativa, ret.tentativa_id)
        gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
        health_check_aprovado.concluido_em = datetime.now(UTC) - timedelta(minutes=8)
        novo = gravar_health_check(db_session, "4x4")

        ret = gerenciador.retomar(db_session, ret.execucao_id)

        assert db_session.get(Tentativa, ret.tentativa_id).hc_id == novo.hc_id


def test_retomar_concorrente_uma_aceita_outra_recusa(
    client: TestClient, db_session: Session, gerenciador
):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    execucao_id = criado["execucao_id"]
    tentativa = db_session.get(Tentativa, uuid.UUID(criado["tentativa_id"]))
    gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
    db_session.commit()

    assert client.post(f"/execucoes/{execucao_id}/retomar").status_code == 201
    recusada = client.post(f"/execucoes/{execucao_id}/retomar")
    assert recusada.status_code == 409
    assert recusada.json()["motivo"] == "tentativa_aberta"


def test_sucesso_via_telemetria_http(client: TestClient, db_session: Session):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    execucao_id = criado["execucao_id"]
    linha_ok = escrever_linha(_sucesso())
    payload_ok = {"linha": linha_ok.strip(), "recebido_em": "2026-10-07T17:00:02Z"}
    client.post("/telemetria", json=payload_ok)
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(execucao_id))
    assert execucao.status == "concluida"


def _tel() -> str:
    return escrever_linha(
        Tel(
            v=1,
            boot=1,
            seq=9,
            t_ms=900,
            tipo="tel",
            estado="running",
            x=0,
            y=0,
            rumo="N",
            bat_mv=7800,
            vel_mm_s=0,
            eixo_longo=None,
        )
    ).strip()


def _sem_leituras(db_session: Session, tentativa_id: uuid.UUID) -> None:
    leituras = db_session.scalars(
        select(LeituraTelemetria).where(LeituraTelemetria.tentativa_id == tentativa_id)
    ).all()
    assert leituras == []


def test_sinal_anterior_a_tentativa_nao_cancela(db_session: Session, gerenciador):
    ret = gerenciador.nova_execucao(db_session, "4x4")
    monitor = MonitorConexao(limite_sem_sinal_s=10)
    monitor.registrar_mensagem(datetime.now(UTC) - timedelta(seconds=30))
    assert gerenciador.verificar_monitores(db_session, monitor) is False
    tentativa = db_session.get(Tentativa, ret.tentativa_id)
    assert tentativa.status == "running"


def test_silencio_grava_link_lost_sem_mensagem(db_session: Session, gerenciador):
    ret = gerenciador.nova_execucao(db_session, "4x4")
    tentativa = db_session.get(Tentativa, ret.tentativa_id)
    tentativa.iniciada_em = datetime.now(UTC) - timedelta(seconds=11)
    assert gravar_se_cancelou(db_session, gerenciador, MonitorConexao(10)) is True
    db_session.expire_all()
    tentativa = db_session.get(Tentativa, ret.tentativa_id)
    execucao = db_session.get(ExecucaoLogica, ret.execucao_id)
    assert tentativa.status == "failed"
    assert tentativa.falha.motivo == "link_lost"
    assert execucao.status == "cancelada"


def test_perda_de_link_na_ingestao_grava_falha_e_descarta_a_linha(
    client: TestClient, db_session: Session
):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa_id = uuid.UUID(criado["tentativa_id"])
    tentativa = db_session.get(Tentativa, tentativa_id)
    tentativa.iniciada_em = datetime.now(UTC) - timedelta(seconds=11)
    db_session.commit()

    resposta = client.post(
        "/telemetria",
        json={"linha": _tel(), "recebido_em": "2026-10-07T17:00:01Z"},
    )
    assert resposta.status_code == 200
    db_session.expire_all()
    tentativa = db_session.get(Tentativa, tentativa_id)
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(criado["execucao_id"]))
    assert tentativa.falha.motivo == "link_lost"
    assert execucao.status == "cancelada"
    _sem_leituras(db_session, tentativa_id)


def test_tempo_excedido_na_ingestao_grava_falha_e_descarta_a_linha(
    client: TestClient, db_session: Session
):
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa_id = uuid.UUID(criado["tentativa_id"])
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(criado["execucao_id"]))
    execucao.iniciada_em = datetime.now(UTC) - timedelta(seconds=601)
    db_session.commit()

    resposta = client.post(
        "/telemetria",
        json={"linha": _tel(), "recebido_em": "2026-10-07T17:00:01Z"},
    )
    assert resposta.status_code == 200
    db_session.expire_all()
    tentativa = db_session.get(Tentativa, tentativa_id)
    execucao = db_session.get(ExecucaoLogica, uuid.UUID(criado["execucao_id"]))
    assert tentativa.falha.motivo == "time_exceeded"
    assert execucao.status == "cancelada"
    _sem_leituras(db_session, tentativa_id)
