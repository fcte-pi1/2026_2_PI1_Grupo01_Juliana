"""Fábricas dos eventos SSE: um payload por evento, no formato do contrato (BACK-07)."""

import json
import uuid
from datetime import UTC, datetime
from decimal import Decimal

import pytest
from app.models import Falha, HealthCheckItem, PassoTrajeto, Tentativa
from app.services.fabrica_eventos import (
    FABRICAS,
    OrigemFalha,
    OrigemStatus,
    OrigemSucesso,
    OrigemTelemetria,
    criar_eventos,
)
from contrato.telemetria import ler_linha

MOMENTO = datetime(2026, 10, 7, 17, 3, 22, tzinfo=UTC)
TENTATIVA_ID = uuid.UUID("7c9e6679-7425-40de-944b-e07fc1f90ae7")

LINHA_TEL = (
    '{"v":1,"boot":7,"seq":43,"t_ms":11400,"tipo":"tel","estado":"running",'
    '"x":0,"y":2,"rumo":"N","bat_mv":7810,"vel_mm_s":210,"eixo_longo":null}'
)


def _passo(**campos) -> PassoTrajeto:
    valores = {
        "passo_id": 41,
        "tentativa_id": TENTATIVA_ID,
        "execucao_id": uuid.uuid4(),
        "seq": 2,
        "x": 0,
        "y": 1,
        "paredes_mask": 9,
        "retomada": False,
        "entrou_em": MOMENTO,
    }
    return PassoTrajeto(**(valores | campos))


def _tentativa(**campos) -> Tentativa:
    valores = {
        "tentativa_id": TENTATIVA_ID,
        "execucao_id": uuid.uuid4(),
        "attempt_index": 1,
        "status": "running",
        "tipo_inicio": "nova",
        "tipo_dip": "4x4",
        "iniciada_em": MOMENTO,
    }
    return Tentativa(**(valores | campos))


def _dados(origem: object) -> dict:
    return json.loads(FABRICAS.criar(origem).dados)


def test_evento_do_passo_leva_id_e_o_tipo_bate_com_o_event():
    evento = FABRICAS.criar(_passo())

    assert evento.tipo == "passo"
    assert evento.id == 41
    assert json.loads(evento.dados) == {
        "tipo": "passo",
        "passo_id": 41,
        "seq": 2,
        "x": 0,
        "y": 1,
        "paredes_mask": 9,
        "retomada": False,
        "entrou_em": "2026-10-07T17:03:22Z",
    }


def test_passo_sem_mascara_gravada_sai_com_zero():
    """A coluna é nulável, mas o contrato exige paredes_mask entre 0 e 15."""
    assert _dados(_passo(paredes_mask=None))["paredes_mask"] == 0


def test_telemetria_leva_mv_porcentagem_rumo_e_eixo_longo():
    origem = OrigemTelemetria(tel=ler_linha(LINHA_TEL), ordem=42, enviado_em=MOMENTO)

    evento = FABRICAS.criar(origem)

    assert evento.tipo == "telemetria"
    # sem linha id: a amostra de telemetria não entra no reenvio
    assert evento.id is None
    assert json.loads(evento.dados) == {
        "tipo": "telemetria",
        "ordem": 42,
        "x": 0,
        "y": 2,
        "bateria": 7810,
        "bateria_pct": 56.0,
        "velocidade": 210,
        "rumo": "N",
        "eixo_longo": None,
        "enviado_em": "2026-10-07T17:03:22Z",
    }


def test_health_check_vem_da_linha_gravada():
    item = HealthCheckItem(
        tentativa_id=TENTATIVA_ID,
        componente="bateria",
        aprovado=True,
        valor_lido=Decimal("7840"),
        verificado_em=MOMENTO,
    )

    assert _dados(item) == {
        "tipo": "health_check",
        "tentativa_id": str(TENTATIVA_ID),
        "componente": "bateria",
        "aprovado": True,
        "valor_lido": 7840.0,
    }


def test_status_junta_os_dois_status_da_adr_013():
    origem = OrigemStatus(tentativa=_tentativa(), status_execucao="em_andamento")

    assert _dados(origem) == {
        "tipo": "status",
        "tentativa_id": str(TENTATIVA_ID),
        "attempt_index": 1,
        "status_tentativa": "running",
        "status_execucao": "em_andamento",
        "tipo_dip": "4x4",
    }


def test_falha_e_terminal_so_quando_a_execucao_deixa_de_andar():
    falha = Falha(
        tentativa_id=TENTATIVA_ID,
        motivo="collision",
        origem="automatica",
        celula_x=1,
        celula_y=2,
        momento_falha=MOMENTO,
    )

    em_andamento = FABRICAS.criar(OrigemFalha(falha=falha, status_execucao="em_andamento"))
    cancelada = FABRICAS.criar(OrigemFalha(falha=falha, status_execucao="cancelada"))

    assert em_andamento.tipo == "falha"
    assert em_andamento.encerra is False
    assert cancelada.encerra is True
    assert json.loads(cancelada.dados)["status_execucao"] == "cancelada"


def test_sucesso_encerra_a_execucao_concluida():
    origem = OrigemSucesso(tentativa=_tentativa(), x=3, y=3, status_execucao="concluida")

    evento = FABRICAS.criar(origem)

    assert evento.encerra is True
    assert json.loads(evento.dados) == {
        "tipo": "sucesso",
        "tentativa_id": str(TENTATIVA_ID),
        "x": 3,
        "y": 3,
        "status_execucao": "concluida",
    }


def test_origem_sem_fabrica_registrada_levanta():
    with pytest.raises(TypeError, match="sem fábrica de evento SSE"):
        FABRICAS.criar(object())


def test_criar_eventos_engole_a_falha_para_nao_desfazer_a_gravacao():
    """Montar o payload acontece dentro da transação: um erro aqui não pode
    custar a linha gravada, só a atualização ao vivo, que o reenvio recupera."""
    assert criar_eventos(object()) == []
