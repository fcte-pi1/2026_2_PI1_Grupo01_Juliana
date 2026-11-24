"""Largar com a bateria abaixo do corte: só quando ela é o único item reprovado."""

import pytest
from app.models import Tentativa
from app.services.gerenciador_execucoes import COMPONENTES_DO_HEALTH_CHECK
from fastapi.testclient import TestClient
from sqlalchemy import select
from sqlalchemy.orm import Session
from tests.integracao.apoio_health_check import gravar_health_check


def _itens(**reprovados: bool) -> dict[str, bool]:
    return {nome: reprovados.get(nome, True) for nome in COMPONENTES_DO_HEALTH_CHECK}


def _health_check_so_bateria(db_session: Session):
    return gravar_health_check(db_session, "4x4", aprovado=False, itens=_itens(bateria=False))


def _comandos(client: TestClient) -> list[dict]:
    from app.main import app

    return [c.model_dump(exclude_none=True) for c in app.state.gerenciador.comandos_pendentes()]


def test_bateria_reprovada_sem_confirmacao_continua_recusada(client, db_session):
    _health_check_so_bateria(db_session)
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4"})
    assert r.status_code == 409
    assert r.json()["motivo"] == "health_check_reprovado"


def test_confirmando_a_bateria_a_execucao_abre_e_o_largar_vai_forcado(client, db_session):
    _health_check_so_bateria(db_session)
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "ignorar_bateria": True})
    assert r.status_code == 201
    assert _comandos(client) == [{"v": 1, "cmd": "largar", "forcar_bateria": True}]
    assert db_session.scalars(select(Tentativa)).one().status == "running"


def test_largada_normal_nao_leva_o_campo_forcar_bateria(client, db_session):
    gravar_health_check(db_session, "4x4", itens=_itens())
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "ignorar_bateria": True})
    assert r.status_code == 201
    assert _comandos(client) == [{"v": 1, "cmd": "largar"}]


@pytest.mark.parametrize("outro", ["tof_esquerdo", "motor_direito", "encoder_esquerdo"])
def test_outro_item_reprovado_nao_e_liberado(client, db_session, outro):
    gravar_health_check(
        db_session, "4x4", aprovado=False, itens=_itens(bateria=False, **{outro: False})
    )
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "ignorar_bateria": True})
    assert r.status_code == 409
    assert r.json()["motivo"] == "health_check_reprovado"
    assert db_session.scalars(select(Tentativa)).all() == []


def test_item_ausente_nao_e_liberado(client, db_session):
    itens = _itens(bateria=False)
    del itens["tof_direito"]
    gravar_health_check(db_session, "4x4", aprovado=False, itens=itens)
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "ignorar_bateria": True})
    assert r.status_code == 409


def test_dip_divergente_continua_recusando(client, db_session):
    _health_check_so_bateria(db_session)
    r = client.post("/execucoes", json={"tipo_labirinto": "8x4", "ignorar_bateria": True})
    assert r.status_code == 409
    assert r.json()["motivo"] == "dip_divergente"


def test_health_check_expirado_continua_recusando(client, db_session):
    from datetime import UTC, datetime, timedelta

    gravar_health_check(
        db_session,
        "4x4",
        aprovado=False,
        concluido_em=datetime.now(UTC) - timedelta(minutes=11),
        itens=_itens(bateria=False),
    )
    r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "ignorar_bateria": True})
    assert r.status_code == 409


def test_retomar_tambem_aceita_a_confirmacao(client, db_session):
    from app.main import app
    from tests.integracao.test_transicoes_execucao import _falha_auto

    gravar_health_check(db_session, "4x4", itens=_itens())
    criado = client.post("/execucoes", json={"tipo_labirinto": "4x4"}).json()
    tentativa = db_session.get(Tentativa, criado["tentativa_id"])
    app.state.gerenciador.aplicar_evento(db_session, tentativa, _falha_auto())
    db_session.commit()

    _health_check_so_bateria(db_session)
    recusada = client.post(f"/execucoes/{criado['execucao_id']}/retomar")
    assert recusada.status_code == 409
    aceita = client.post(
        f"/execucoes/{criado['execucao_id']}/retomar", json={"ignorar_bateria": True}
    )
    assert aceita.status_code == 201
    assert _comandos(client) == [{"v": 1, "cmd": "largar", "forcar_bateria": True}]
