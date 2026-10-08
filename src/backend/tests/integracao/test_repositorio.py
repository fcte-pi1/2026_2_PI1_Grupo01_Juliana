import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

import pytest
from app.models import ExecucaoLogica, Labirinto, Tentativa
from app.repositories import FiltroExecucoes, Repositorio
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError


def _labirinto_id(db_session, tipo: str) -> int:
    lab_id = db_session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == tipo))
    assert lab_id is not None
    return lab_id


def _nova_execucao(
    db_session,
    *,
    tipo: str = "4x4",
    iniciada_em: datetime | None = None,
) -> ExecucaoLogica:
    repo = Repositorio(db_session)
    execucao = ExecucaoLogica(
        execucao_id=uuid.uuid4(),
        labirinto_id=_labirinto_id(db_session, tipo),
        status="em_andamento",
        tentativas_usadas=1,
        iniciada_em=iniciada_em or datetime.now(UTC),
    )
    repo.salvar(execucao)
    return execucao


def _nova_tentativa(
    db_session,
    execucao: ExecucaoLogica,
    *,
    attempt_index: int = 1,
    status: str = "success",
    tempo_s: Decimal | None = Decimal("42.5"),
) -> Tentativa:
    repo = Repositorio(db_session)
    tentativa = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=attempt_index,
        status=status,
        tipo_inicio="nova" if attempt_index == 1 else "retomada",
        iniciada_em=datetime.now(UTC),
        encerrada_em=datetime.now(UTC) if status in ("success", "failed") else None,
        tempo_s=tempo_s if status == "success" else None,
    )
    repo.salvar(tentativa)
    return tentativa


def test_salvar_execucao_e_tentativa(db_session):
    repo = Repositorio(db_session)
    execucao = _nova_execucao(db_session)
    tentativa = _nova_tentativa(db_session, execucao, status="health-check", tempo_s=None)
    tentativa.status = "running"
    repo.salvar(tentativa)

    carregada = repo.buscar_execucao(execucao.execucao_id)
    assert carregada is not None
    assert carregada.labirinto.tipo == "4x4"
    assert len(carregada.tentativas) == 1
    assert carregada.tentativas[0].tentativa_id == tentativa.tentativa_id


def test_buscar_execucao_ordena_tentativas_por_attempt_index(db_session):
    repo = Repositorio(db_session)
    execucao = _nova_execucao(db_session)
    execucao.tentativas_usadas = 3
    repo.salvar(execucao)
    _nova_tentativa(db_session, execucao, attempt_index=3, status="failed", tempo_s=None)
    _nova_tentativa(db_session, execucao, attempt_index=1, status="failed", tempo_s=None)
    _nova_tentativa(db_session, execucao, attempt_index=2, status="failed", tempo_s=None)

    carregada = repo.buscar_execucao(execucao.execucao_id)
    assert carregada is not None
    indices = [t.attempt_index for t in carregada.tentativas]
    assert indices == [1, 2, 3]


def test_listar_execucoes_ordem_decrescente_e_filtro(db_session):
    repo = Repositorio(db_session)
    base = datetime.now(UTC)
    mais_antiga = _nova_execucao(
        db_session, tipo="8x4", iniciada_em=base - timedelta(hours=2)
    )
    mais_recente = _nova_execucao(
        db_session, tipo="4x4", iniciada_em=base - timedelta(minutes=5)
    )
    _nova_execucao(db_session, tipo="8x4", iniciada_em=base - timedelta(hours=1))

    todas = repo.listar_execucoes()
    ids = [e.execucao_id for e in todas]
    assert ids.index(mais_recente.execucao_id) < ids.index(mais_antiga.execucao_id)

    so_oito = repo.listar_execucoes(FiltroExecucoes(tipo_labirinto="8x4"))
    assert len(so_oito) == 2
    assert all(e.labirinto.tipo == "8x4" for e in so_oito)


def test_melhor_tempo_menor_success_por_tipo(db_session):
    repo = Repositorio(db_session)
    exec_a = _nova_execucao(db_session, tipo="4x4")
    _nova_tentativa(db_session, exec_a, tempo_s=Decimal("50"))
    exec_b = _nova_execucao(db_session, tipo="4x4")
    _nova_tentativa(db_session, exec_b, tempo_s=Decimal("30"))
    exec_oito = _nova_execucao(db_session, tipo="8x4")
    _nova_tentativa(db_session, exec_oito, tempo_s=Decimal("10"))

    assert repo.melhor_tempo("4x4") == Decimal("30")
    assert repo.melhor_tempo("8x4") == Decimal("10")


def test_melhor_tempo_ignora_failed_e_retorna_none_sem_success(db_session):
    repo = Repositorio(db_session)
    execucao = _nova_execucao(db_session, tipo="12x4")
    _nova_tentativa(db_session, execucao, status="failed", tempo_s=None)

    assert repo.melhor_tempo("12x4") is None


@pytest.mark.parametrize("tipo", ["4x4", "8x4", "12x4"])
def test_melhor_tempo_labirintos_seed(db_session, tipo):
    assert Repositorio(db_session).melhor_tempo(tipo) is None


def test_duas_tentativas_abertas_mesma_execucao_falha_no_banco(db_session):
    """RF18: índice parcial uq_tentativa_aberta_por_execucao no PostgreSQL."""
    repo = Repositorio(db_session)
    execucao = _nova_execucao(db_session)
    _nova_tentativa(db_session, execucao, attempt_index=1, status="running", tempo_s=None)

    segunda_aberta = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=2,
        status="health-check",
        tipo_inicio="retomada",
        iniciada_em=datetime.now(UTC),
    )
    with pytest.raises(IntegrityError):
        repo.salvar(segunda_aberta)


def test_tentativa_aberta_apos_anterior_finalizada_e_permitida(db_session):
    repo = Repositorio(db_session)
    execucao = _nova_execucao(db_session)
    execucao.tentativas_usadas = 2
    repo.salvar(execucao)
    _nova_tentativa(db_session, execucao, attempt_index=1, status="failed", tempo_s=None)
    segunda = _nova_tentativa(
        db_session, execucao, attempt_index=2, status="running", tempo_s=None
    )

    carregada = repo.buscar_execucao(execucao.execucao_id)
    assert carregada is not None
    abertas = [t for t in carregada.tentativas if t.status in ("health-check", "running")]
    assert len(abertas) == 1
    assert abertas[0].tentativa_id == segunda.tentativa_id
