"""Caso 4.1 do 7.4: mensagens malformadas são descartadas e o serviço segue respondendo.

O catálogo cobre cada regra de descarte da seção "Validação no backend" do 4.4.1
(RNF-B09), enviada pelo POST /telemetria com uma tentativa aberta num 4x4.
"""

import json
import logging

import pytest
from app.models import Falha, HealthCheck, LeituraTelemetria, ParedeCelula, PassoTrajeto
from sqlalchemy import func, select

RECEBIDO_EM = "2026-10-25T12:00:00-03:00"
TEL = {
    "v": 1,
    "boot": 7,
    "seq": 1,
    "t_ms": 1000,
    "tipo": "tel",
    "estado": "running",
    "x": 0,
    "y": 0,
    "rumo": "N",
    "bat_mv": 7810,
    "vel_mm_s": 210,
    "eixo_longo": None,
}
EVENTO = {"v": 1, "boot": 7, "seq": 1, "t_ms": 1000}


def _json(campos: dict) -> str:
    return json.dumps(campos, separators=(",", ":"))


def _tel(**mudancas) -> str:
    return _json(TEL | mudancas)


def _sem(campo: str) -> str:
    return _json({k: v for k, v in TEL.items() if k != campo})


def _evento(**campos) -> str:
    return _json(EVENTO | campos)


def _falha(**campos) -> str:
    return _evento(tipo="falha", x=0, y=0, **campos)


# (regra do 4.4.1, linha)
CATALOGO = {
    # Passa de 256 bytes ou não é JSON
    "300_bytes": ("tamanho", _tel() + " " * (300 - len(_tel()))),
    "json_cortado": ("nao_json", _tel()[:40]),
    "texto_solto": ("nao_json", "bateria ok"),
    "lixo_binario": ("nao_json", "��\x00\x7f"),
    "lista_json": ("nao_json", "[1,2,3]"),
    "null_json": ("nao_json", "null"),
    # v diferente de 1 ou tipo desconhecido
    "versao_2": ("versao_ou_tipo", _tel(v=2)),
    "sem_versao": ("versao_ou_tipo", _sem("v")),
    "tipo_heartbeat": ("versao_ou_tipo", _tel(tipo="heartbeat")),
    "sem_tipo": ("versao_ou_tipo", _sem("tipo")),
    # Campo faltando, tipo errado ou fora da faixa
    "sem_bat_mv": ("campo", _sem("bat_mv")),
    "x_texto": ("campo", _tel(x="3")),
    "bat_mv_decimal": ("campo", _tel(bat_mv=7810.5)),
    "bat_mv_acima": ("campo", _tel(bat_mv=10_001)),
    "vel_negativa": ("campo", _tel(vel_mm_s=-1)),
    "rumo_ne": ("campo", _tel(rumo="NE")),
    "estado_parado": ("campo", _tel(estado="parado")),
    "eixo_z": ("campo", _tel(eixo_longo="z")),
    "y_negativo": ("campo", _tel(y=-1)),
    "seq_negativo": ("campo", _tel(seq=-1)),
    "paredes_16": ("campo", _evento(tipo="passo", x=0, y=0, paredes=16)),
    "hc_tof_traseiro": (
        "campo",
        _evento(tipo="hc_item", componente="tof_traseiro", aprovado=True, valor=None),
    ),
    "hc_dip_invalido_aprovado": (
        "campo",
        _evento(tipo="hc_resultado", aprovado=True, tipo_dip="invalido", inicio="nova"),
    ),
    # origem, motivo e componente da falha não combinam (Tabela 8)
    "falha_web_com_motivo": ("falha", _falha(motivo="collision", origem="web", componente=None)),
    "falha_boot_sem_operador": ("falha", _falha(motivo="stuck", origem="boot", componente=None)),
    "falha_auto_time_exceeded": (
        "falha",
        _falha(motivo="time_exceeded", origem="automatica", componente=None),
    ),
    "falha_componente_sem_componente": (
        "falha",
        _falha(motivo="falha_componente", origem="automatica", componente=None),
    ),
    "falha_collision_com_componente": (
        "falha",
        _falha(motivo="collision", origem="automatica", componente="tof_direito"),
    ),
    # Célula fora dos limites do labirinto da execução (4x4)
    "tel_x_4": ("celula", _tel(x=4)),
    "tel_y_11": ("celula", _tel(y=11)),
    "passo_fora": ("celula", _evento(tipo="passo", x=0, y=4, paredes=0)),
    "falha_fora": (
        "celula",
        _evento(tipo="falha", x=5, y=0, motivo="stuck", origem="automatica", componente=None),
    ),
    "sucesso_fora": ("celula", _evento(tipo="sucesso", x=7, y=3)),
}
REGRAS = {"tamanho", "nao_json", "versao_ou_tipo", "campo", "falha", "celula"}
TABELAS = (LeituraTelemetria, PassoTrajeto, ParedeCelula, Falha, HealthCheck)


@pytest.fixture
def tentativa_4x4(fabrica):
    execucao = fabrica.execucao(tipo="4x4", status="em_andamento", tentativas_usadas=1)
    return fabrica.tentativa(execucao, status="running")


def _enviar(client, linha: str):
    return client.post("/telemetria", json={"linha": linha, "recebido_em": RECEBIDO_EM})


def _contagens(db_session) -> dict[str, int]:
    return {
        tabela.__tablename__: db_session.scalar(select(func.count()).select_from(tabela))
        for tabela in TABELAS
    }


def test_catalogo_cobre_todas_as_regras_do_contrato():
    assert {regra for regra, _ in CATALOGO.values()} == REGRAS
    assert len(CATALOGO["300_bytes"][1].encode()) == 300


@pytest.mark.parametrize("linha", [linha for _, linha in CATALOGO.values()], ids=CATALOGO.keys())
def test_malformada_e_descartada_sem_gravar_nada(client, db_session, tentativa_4x4, caplog, linha):
    antes = _contagens(db_session)

    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, linha)

    assert resposta.status_code == 422
    assert isinstance(resposta.json()["detail"], str)
    assert resposta.json()["comandos"] == []
    assert any(r.name == "app.telemetria" and r.levelno == logging.WARNING for r in caplog.records)
    assert _contagens(db_session) == antes
    db_session.refresh(tentativa_4x4)
    assert tentativa_4x4.status == "running"


def test_rajada_intercalada_descarta_100_por_cento_e_segue_respondendo(
    client, db_session, tentativa_4x4
):
    descartadas = 0
    aceitas = 0

    for seq, (_, linha) in enumerate(CATALOGO.values(), start=1):
        if _enviar(client, linha).status_code == 422:
            descartadas += 1
        valida = _tel(seq=seq, t_ms=1000 + 100 * seq)
        if _enviar(client, valida).status_code == 200:
            aceitas += 1

    assert descartadas == len(CATALOGO)
    assert aceitas == len(CATALOGO)
    assert _contagens(db_session)["leitura_telemetria"] == len(CATALOGO)
    assert client.get("/back-health").status_code == 200
    db_session.refresh(tentativa_4x4)
    assert tentativa_4x4.status == "running"
