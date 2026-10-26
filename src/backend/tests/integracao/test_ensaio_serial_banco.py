"""Conferência no banco do ensaio serial (caso 4.16)."""

import json
from datetime import timedelta

import pytest
from app.models import LeituraTelemetria, PassoTrajeto
from scripts.ensaio_serial import EventoPonte, conferir_banco
from sqlalchemy import select

BOOT = 41
CELULAS = [(0, 0), (0, 1), (0, 2), (1, 2)]


def _rx(hora, tipo, seq, **campos):
    dado = {"v": 1, "boot": BOOT, "seq": seq, "t_ms": seq * 100, "tipo": tipo, **campos}
    return EventoPonte(hora=hora, tipo="RX", conteudo=json.dumps(dado, separators=(",", ":")))


@pytest.fixture
def ensaio(fabrica, db_session):
    """Tentativa encerrada por tempo, com 4 passos, 6 leituras e RX coerentes."""
    execucao = fabrica.execucao()
    tentativa = fabrica.tentativa(execucao, status="failed")
    fabrica.health_check(tentativa).boot = BOOT
    fabrica.passos(tentativa, CELULAS)
    fabrica.leituras(tentativa, 6)
    fabrica.falha(tentativa, motivo="time_exceeded")
    db_session.flush()

    inicio, fim = tentativa.iniciada_em, tentativa.encerrada_em
    eventos = [
        _rx(inicio - timedelta(seconds=2, milliseconds=10 * i), "hc_item", i) for i in range(8)
    ]
    eventos.append(_rx(inicio - timedelta(seconds=1), "tel", 8))
    eventos.append(_rx(inicio + timedelta(milliseconds=20), "tel", 9))
    seq = 10
    for indice, (x, y) in enumerate(CELULAS):
        hora = inicio + timedelta(seconds=1 + indice)
        eventos.append(_rx(hora, "passo", seq, x=x, y=y, paredes=5))
        eventos.append(_rx(hora + timedelta(milliseconds=100), "tel", seq + 1))
        seq += 2
    eventos.append(_rx(inicio + timedelta(seconds=10), "tel", seq))
    eventos.append(_rx(inicio + timedelta(seconds=11), "tel", seq + 1))
    eventos.append(_rx(fim + timedelta(seconds=1), "tel", seq + 2))
    eventos.append(EventoPonte(hora=fim, tipo="API", conteudo="200 comandos=0"))
    return tentativa, eventos


def test_conferir_banco_aprova_quando_tudo_da_janela_foi_gravado(ensaio, db_session):
    tentativa, eventos = ensaio

    resultado = conferir_banco(db_session, eventos, BOOT)

    assert resultado.problemas == []
    assert resultado.aprovado
    assert resultado.execucao_id == tentativa.execucao_id
    assert resultado.motivo_encerramento == "time_exceeded"
    assert resultado.antes == {"hc_item": 8, "tel": 1}
    assert resultado.dentro == {"passo": 4, "tel": 6}
    assert resultado.depois == {"tel": 1}
    assert resultado.gravados == {"tel": 6, "passo": 4, "hc_item": 8}
    assert [json.loads(e.conteudo)["seq"] for e in resultado.fronteira] == [9]


def test_conferir_banco_reprova_sem_um_passo(ensaio, db_session):
    tentativa, eventos = ensaio
    passo = db_session.scalars(
        select(PassoTrajeto)
        .where(PassoTrajeto.execucao_id == tentativa.execucao_id)
        .order_by(PassoTrajeto.seq.desc())
        .limit(1)
    ).one()
    db_session.delete(passo)
    db_session.flush()

    resultado = conferir_banco(db_session, eventos, BOOT)

    assert not resultado.aprovado
    assert resultado.problemas == ["passo 4 da janela não gravado: célula (1, 2)"]


def test_conferir_banco_reprova_sem_uma_leitura(ensaio, db_session):
    tentativa, eventos = ensaio
    leitura = db_session.scalars(
        select(LeituraTelemetria).where(LeituraTelemetria.tentativa_id == tentativa.tentativa_id)
    ).first()
    db_session.delete(leitura)
    db_session.flush()

    resultado = conferir_banco(db_session, eventos, BOOT)

    assert not resultado.aprovado
    assert resultado.problemas == [
        "leituras gravadas (5) diferente de tel RX na janela (6, mais 1 na fronteira)"
    ]


def test_conferir_banco_reprova_tentativa_aberta_e_boot_sem_tentativa(fabrica, db_session):
    execucao = fabrica.execucao(status="em_andamento")
    tentativa = fabrica.tentativa(execucao, status="running")
    fabrica.health_check(tentativa).boot = BOOT
    db_session.flush()

    assert "tentativa ainda aberta" in conferir_banco(db_session, [], BOOT).problemas
    assert conferir_banco(db_session, [], BOOT + 1).problemas == [
        f"nenhuma tentativa com health-check do boot {BOOT + 1}"
    ]
