"""Monitores de link e de tempo com relógio falso (BACK-05, RF31, RF32, RNF-B08).

A varredura de fundo roda a cada `INTERVALO_S`. Aqui ela é simulada passada a
passada, avançando o relógio do monitor, sem esperar 10 s nem 10 min de verdade.
"""

from __future__ import annotations

import json
import threading
from datetime import UTC, datetime, timedelta

import pytest
from app.models import ExecucaoLogica, Tentativa
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.ingestao_telemetria import IngestaoTelemetria
from app.services.monitor_conexao import MonitorConexao
from app.services.vigilancia_monitores import INTERVALO_S, gravar_se_cancelou, vigiar
from contrato.telemetria import Deduplicador, EntradaPonte, RelogioDoRobo, Tel, escrever_linha
from sqlalchemy.orm import Session

LIMITE_SEM_SINAL_S = 10
TEMPO_MAXIMO_S = 600
INICIO = datetime(2026, 10, 7, 17, 0, tzinfo=UTC)


class RelogioFalso:
    def __init__(self, inicio: datetime) -> None:
        self.agora = inicio

    def __call__(self) -> datetime:
        return self.agora

    def avancar(self, segundos: float) -> None:
        self.agora += timedelta(seconds=segundos)


class Espia:
    """Faz o papel do PublicadorSSE: guarda o que seria enviado ao stream."""

    def __init__(self) -> None:
        self.publicados: list[tuple[str, list]] = []

    def publicar(self, execucao_id, eventos) -> None:
        self.publicados.append((str(execucao_id), list(eventos)))

    def tipos(self) -> list[str]:
        return [evento.tipo for _, eventos in self.publicados for evento in eventos]


@pytest.fixture
def relogio() -> RelogioFalso:
    return RelogioFalso(INICIO)


@pytest.fixture
def monitor(relogio) -> MonitorConexao:
    return MonitorConexao(LIMITE_SEM_SINAL_S, relogio=relogio)


@pytest.fixture
def gerenciador() -> GerenciadorExecucoes:
    return GerenciadorExecucoes(TEMPO_MAXIMO_S)


@pytest.fixture
def tentativa(fabrica) -> Tentativa:
    """Tentativa running aberta em INICIO, o instante zero do relógio falso."""
    execucao = fabrica.execucao(status="em_andamento", iniciada_em=INICIO, tentativas_usadas=1)
    return fabrica.tentativa(execucao, status="running")


def _varrer_ate_cancelar(
    sessao, gerenciador, monitor, relogio, *, com_sinal=False, publicador=None
):
    """Uma passada por INTERVALO_S até o monitor disparar. Devolve o instante do disparo."""
    for _ in range(TEMPO_MAXIMO_S + 10):
        if gravar_se_cancelou(sessao, gerenciador, monitor, publicador):
            return relogio.agora
        if com_sinal:
            monitor.registrar_mensagem()
        relogio.avancar(INTERVALO_S)
    raise AssertionError("o monitor não disparou")


def _recarregar(db_session: Session, tentativa: Tentativa) -> tuple[Tentativa, ExecucaoLogica]:
    db_session.expire_all()
    tentativa = db_session.get(Tentativa, tentativa.tentativa_id)
    return tentativa, db_session.get(ExecucaoLogica, tentativa.execucao_id)


def test_varredura_roda_ao_menos_uma_vez_por_segundo():
    assert INTERVALO_S <= 1.0


def test_vigiar_passa_logo_ao_iniciar():
    parar = threading.Event()
    chamadas = []

    def passar():
        chamadas.append(1)
        parar.set()

    vigiar(parar, passar)
    assert chamadas == [1]


@pytest.mark.parametrize("defasagem_s", [0.0, 0.4, 0.99])
def test_link_lost_dispara_ate_1s_depois_do_limite(
    db_session, gerenciador, monitor, relogio, tentativa, defasagem_s
):
    relogio.avancar(2.3)
    monitor.registrar_mensagem()
    ultima_mensagem = relogio.agora
    # a varredura não está alinhada com a última mensagem
    relogio.avancar(defasagem_s)

    disparo = _varrer_ate_cancelar(db_session, gerenciador, monitor, relogio)

    atraso = (disparo - ultima_mensagem).total_seconds() - LIMITE_SEM_SINAL_S
    assert 0 <= atraso <= 1
    tentativa, execucao = _recarregar(db_session, tentativa)
    assert tentativa.status == "failed"
    assert tentativa.falha.motivo == "link_lost"
    assert tentativa.falha.origem == "automatica"
    # na tentativa 1 ou 2 a execução segue aberta para a retomada
    assert execucao.status == "em_andamento"


@pytest.mark.parametrize("defasagem_s", [0.0, 0.5])
def test_time_exceeded_dispara_ate_1s_depois_dos_600s(
    db_session, gerenciador, monitor, relogio, tentativa, defasagem_s
):
    # robô falando o tempo todo: só o limite de tempo pode disparar
    relogio.avancar(TEMPO_MAXIMO_S - 5 + defasagem_s)
    monitor.registrar_mensagem()

    disparo = _varrer_ate_cancelar(db_session, gerenciador, monitor, relogio, com_sinal=True)

    atraso = (disparo - INICIO).total_seconds() - TEMPO_MAXIMO_S
    assert 0 <= atraso <= 1
    tentativa, execucao = _recarregar(db_session, tentativa)
    assert tentativa.status == "failed"
    assert tentativa.falha.motivo == "time_exceeded"
    assert execucao.status == "cancelada"


def test_reconexao_com_9s_de_silencio_nao_dispara(
    db_session, gerenciador, monitor, relogio, tentativa
):
    monitor.registrar_mensagem()
    for _ in range(3):
        relogio.avancar(9)
        assert gravar_se_cancelou(db_session, gerenciador, monitor) is False
        # a linha que reconecta zera a contagem
        monitor.registrar_mensagem()

    tentativa, execucao = _recarregar(db_session, tentativa)
    assert tentativa.status == "running"
    assert tentativa.falha is None
    assert execucao.status == "em_andamento"


@pytest.mark.parametrize(
    ("avanco_s", "motivo", "status_execucao"),
    [
        (LIMITE_SEM_SINAL_S, "link_lost", "em_andamento"),
        (TEMPO_MAXIMO_S, "time_exceeded", "cancelada"),
    ],
)
def test_disparo_publica_status_e_falha(
    db_session, gerenciador, monitor, relogio, tentativa, avanco_s, motivo, status_execucao
):
    espia = Espia()
    if motivo == "time_exceeded":
        relogio.avancar(avanco_s)
        monitor.registrar_mensagem()
    else:
        monitor.registrar_mensagem()
        relogio.avancar(avanco_s)

    assert gravar_se_cancelou(db_session, gerenciador, monitor, espia) is True

    [(execucao_id, eventos)] = espia.publicados
    assert execucao_id == str(tentativa.execucao_id)
    status, falha = eventos
    assert status.tipo == "status"
    assert json.loads(status.dados)["status_tentativa"] == "failed"
    assert json.loads(status.dados)["status_execucao"] == status_execucao
    assert falha.tipo == "falha"
    assert json.loads(falha.dados)["motivo"] == motivo
    # a falha que cancela é a última do stream; o status vem antes para não se perder
    assert status.encerra is False
    assert falha.encerra is (status_execucao == "cancelada")


def test_sem_disparo_nao_publica(db_session, gerenciador, monitor, relogio, tentativa):
    espia = Espia()
    monitor.registrar_mensagem()
    relogio.avancar(9)

    assert gravar_se_cancelou(db_session, gerenciador, monitor, espia) is False
    assert espia.publicados == []


def test_erro_ao_publicar_nao_desfaz_a_falha(db_session, gerenciador, monitor, relogio, tentativa):
    class PublicadorQuebrado:
        def publicar(self, execucao_id, eventos) -> None:
            raise RuntimeError("publicador fora do ar")

    relogio.avancar(LIMITE_SEM_SINAL_S)
    assert gravar_se_cancelou(db_session, gerenciador, monitor, PublicadorQuebrado()) is True

    tentativa, _ = _recarregar(db_session, tentativa)
    assert tentativa.falha.motivo == "link_lost"


def test_ingestao_tambem_publica_quando_a_linha_chega_depois_do_silencio(
    db_session, gerenciador, monitor, relogio, tentativa
):
    espia = Espia()
    ingestao = IngestaoTelemetria(
        Deduplicador(), RelogioDoRobo(), monitor, espia, relogio_backend=relogio
    )
    monitor.registrar_mensagem()
    relogio.avancar(LIMITE_SEM_SINAL_S + 0.5)
    linha = escrever_linha(
        Tel(
            v=1,
            boot=1,
            seq=1,
            t_ms=100,
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

    ingestao.processar(
        db_session, EntradaPonte(linha=linha, recebido_em=relogio.agora), gerenciador
    )

    assert espia.tipos() == ["status", "falha"]
    tentativa, _ = _recarregar(db_session, tentativa)
    assert tentativa.falha.motivo == "link_lost"
