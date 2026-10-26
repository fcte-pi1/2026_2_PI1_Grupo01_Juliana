"""Abertura da execução logo depois do health-check no ensaio serial (caso 4.16)."""

from datetime import UTC, datetime, timedelta

import pytest
from scripts.ensaio_serial import FalhaAbertura, abrir_execucao
from tests.integracao.apoio_health_check import gravar_health_check


def test_sem_health_check_novo_da_timeout(client, db_session):
    inicio = datetime.now(UTC)
    gravar_health_check(db_session, "4x4", concluido_em=inicio - timedelta(seconds=5))
    esperas = []

    with pytest.raises(FalhaAbertura, match="nenhum health-check"):
        abrir_execucao(client, "4x4", 0.5, inicio, esperas.append)

    assert esperas == [0.1] * 4
    assert client.get("/execucoes/em-andamento").status_code == 204


def test_health_check_gravado_depois_do_inicio_abre(client, db_session):
    inicio = datetime.now(UTC)
    esperas = []

    def dormir(segundos):
        esperas.append(segundos)
        if len(esperas) == 2:
            gravar_health_check(db_session, "4x4", concluido_em=inicio + timedelta(seconds=1))

    aberta = abrir_execucao(client, "4x4", 5, inicio, dormir)

    assert len(esperas) == 2
    em_andamento = client.get("/execucoes/em-andamento").json()
    assert em_andamento["execucao_id"] == str(aberta.execucao_id)
    assert em_andamento["tentativas"][0]["tentativa_id"] == str(aberta.tentativa_id)


def test_health_check_de_outro_labirinto_recusa(client, db_session):
    inicio = datetime.now(UTC)
    gravar_health_check(db_session, "8x4", concluido_em=inicio + timedelta(seconds=1))

    with pytest.raises(FalhaAbertura, match="409"):
        abrir_execucao(client, "4x4", 1, inicio, lambda _: None)
