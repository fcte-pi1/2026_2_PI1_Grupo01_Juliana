"""RNF-B11: a ingestão aguenta a rajada de telemetria sem atraso acumulado."""

import json
import math
import time
import uuid
from datetime import UTC, datetime

from app.models import ExecucaoLogica, Labirinto, Tentativa
from app.repositories import Repositorio
from sqlalchemy import select

TOTAL = 600
JANELA = 100
P95_MAXIMO_S = 0.100
RECEBIDO_EM = "2026-10-08T12:00:00+00:00"


def _p95(tempos: list[float]) -> float:
    """Percentil 95 pelo método do posto mais próximo."""
    ordenados = sorted(tempos)
    return ordenados[math.ceil(0.95 * len(ordenados)) - 1]


def _mensagem(seq: int) -> str:
    """tel nos índices pares e passo nos ímpares, alternando entre (0,0) e (0,1)."""
    base = {"v": 1, "boot": 50, "seq": seq, "t_ms": 1000 + 100 * seq}
    if seq % 2 == 0:
        campos = {
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 7800,
            "vel_mm_s": 200,
            "eixo_longo": None,
        }
    else:
        campos = {"tipo": "passo", "x": 0, "y": (seq // 2) % 2, "paredes": 9}
    return json.dumps({**base, **campos}, separators=(",", ":"))


def _abrir_tentativa(db_session) -> None:
    labirinto_id = db_session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == "4x4"))
    execucao = ExecucaoLogica(
        execucao_id=uuid.uuid4(),
        labirinto_id=labirinto_id,
        status="em_andamento",
        tentativas_usadas=1,
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(execucao)
    tentativa = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=1,
        status="running",
        tipo_inicio="nova",
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(tentativa)


def test_600_mensagens_sem_espera_ficam_abaixo_de_100_ms_sem_crescer(client, db_session):
    _abrir_tentativa(db_session)

    tempos = []
    for seq in range(TOTAL):
        corpo = {"linha": _mensagem(seq), "recebido_em": RECEBIDO_EM}
        inicio = time.perf_counter()
        resposta = client.post("/telemetria", json=corpo)
        tempos.append(time.perf_counter() - inicio)
        assert resposta.status_code == 200, resposta.text

    p95 = _p95(tempos)
    p95_inicio = _p95(tempos[:JANELA])
    p95_fim = _p95(tempos[-JANELA:])
    assert p95 < P95_MAXIMO_S, f"p95 = {p95 * 1000:.1f} ms"
    assert p95_fim <= 2 * p95_inicio, (
        f"p95 cresceu de {p95_inicio * 1000:.1f} ms para {p95_fim * 1000:.1f} ms"
    )
