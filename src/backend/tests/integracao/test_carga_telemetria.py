"""RNF-B11: a ingestão aguenta a rajada de telemetria sem atraso acumulado."""

import time
import uuid
from datetime import UTC, datetime

from app.models import ExecucaoLogica, Labirinto, Tentativa
from app.repositories import Repositorio
from scripts.carga_telemetria import P95_MAXIMO_S, _p95, mensagem, sem_crescimento
from sqlalchemy import select

TOTAL = 600
JANELA = 100
RECEBIDO_EM = "2026-10-08T12:00:00+00:00"


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
        corpo = {"linha": mensagem(50, seq, 1000 + 100 * seq), "recebido_em": RECEBIDO_EM}
        inicio = time.perf_counter()
        resposta = client.post("/telemetria", json=corpo)
        tempos.append(time.perf_counter() - inicio)
        assert resposta.status_code == 200, resposta.text

    p95 = _p95(tempos)
    p95_inicio = _p95(tempos[:JANELA])
    p95_fim = _p95(tempos[-JANELA:])
    assert p95 < P95_MAXIMO_S, f"p95 = {p95 * 1000:.1f} ms"
    assert sem_crescimento(tempos, JANELA), (
        f"p95 cresceu de {p95_inicio * 1000:.1f} ms para {p95_fim * 1000:.1f} ms"
    )
