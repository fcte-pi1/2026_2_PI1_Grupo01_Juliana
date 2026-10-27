"""Caso 4.6 do 7.4: o histórico é o mesmo antes e depois de reiniciar o servidor (RNF-B05).

O reinício é simulado de verdade: grava com commit, lê pela API, encerra o lifespan e
descarta o engine, depois sobe a aplicação de novo com outro engine e compara as respostas.
"""

import pytest
from app.db import get_db
from app.main import app
from fastapi.testclient import TestClient
from sqlalchemy import create_engine, text
from sqlalchemy.orm import sessionmaker
from tests.conftest import TEST_DATABASE_URL
from tests.integracao.conftest import Fabrica

TABELAS_DA_EXECUCAO = "execucao_logica, health_check, tentativa_rejeitada"


@pytest.fixture
def banco_com_commit(engine):
    """Dados gravados de verdade no banco de teste, apagados no fim."""
    yield
    with engine.begin() as conexao:
        conexao.execute(text(f"TRUNCATE {TABELAS_DA_EXECUCAO} RESTART IDENTITY CASCADE"))


def _subir_servidor():
    """Um servidor novo: engine, sessões e estado em memória (lifespan) próprios."""
    engine_novo = create_engine(TEST_DATABASE_URL, pool_pre_ping=True)
    sessoes = sessionmaker(bind=engine_novo, autocommit=False, autoflush=False)

    def banco():
        sessao = sessoes()
        try:
            yield sessao
        finally:
            sessao.close()

    app.dependency_overrides[get_db] = banco
    cliente = TestClient(app)
    cliente.__enter__()
    return cliente, engine_novo


def _derrubar_servidor(cliente, engine_novo):
    cliente.__exit__(None, None, None)
    engine_novo.dispose()
    app.dependency_overrides.clear()


def _historico(cliente):
    lista = cliente.get("/execucoes", params={"limit": 100})
    assert lista.status_code == 200
    detalhes = {}
    for item in lista.json():
        detalhe = cliente.get(f"/execucoes/{item['execucao_id']}")
        assert detalhe.status_code == 200
        detalhes[item["execucao_id"]] = detalhe.json()
    return lista.json(), lista.headers["X-Total-Count"], detalhes


def test_historico_identico_antes_e_depois_de_reiniciar_o_servidor(engine, banco_com_commit):
    with sessionmaker(bind=engine)() as sessao:
        fabrica = Fabrica(sessao)
        fabrica.execucao_12x4_com_3_tentativas(passos_por_tentativa=20, leituras_por_tentativa=50)
        fabrica.execucao_us28_interrupcoes_operador()
        sessao.commit()

    primeiro, engine_1 = _subir_servidor()
    try:
        gerenciador_1 = primeiro.app.state.gerenciador
        lista_antes, total_antes, detalhes_antes = _historico(primeiro)
    finally:
        _derrubar_servidor(primeiro, engine_1)

    assert int(total_antes) == 2
    assert len(detalhes_antes) == 2
    assert all(len(d["tentativas"]) == 3 for d in detalhes_antes.values())
    assert all(d["trajeto"] for d in detalhes_antes.values())

    segundo, engine_2 = _subir_servidor()
    try:
        # o estado em memória do primeiro servidor não sobrevive ao reinício
        assert segundo.app.state.gerenciador is not gerenciador_1
        lista_depois, total_depois, detalhes_depois = _historico(segundo)
    finally:
        _derrubar_servidor(segundo, engine_2)

    assert total_depois == total_antes
    assert lista_depois == lista_antes
    assert detalhes_depois == detalhes_antes
