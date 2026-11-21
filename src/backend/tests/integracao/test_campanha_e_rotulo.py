"""Rótulo da execução, resumo da campanha e exportação (coleta de dados pelo front)."""

import csv
import io
from datetime import UTC, datetime, timedelta
from decimal import Decimal

import pytest
from tests.integracao.apoio_health_check import gravar_health_check

INICIO = datetime(2026, 11, 21, 12, 0, tzinfo=UTC)


@pytest.fixture
def health_check_aprovado(db_session):
    return gravar_health_check(db_session, "4x4")


class TestRotulo:
    def test_nova_execucao_guarda_configuracao_e_observacao(self, client, health_check_aprovado):
        r = client.post(
            "/execucoes",
            json={"tipo_labirinto": "4x4", "configuracao": "  paredes A ", "observacao": "QA-05"},
        )
        assert r.status_code == 201

        detalhe = client.get(f"/execucoes/{r.json()['execucao_id']}").json()
        assert detalhe["configuracao"] == "paredes A"
        assert detalhe["observacao"] == "QA-05"

    def test_rotulo_e_opcional_e_vazio_vira_nulo(self, client, health_check_aprovado):
        r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "configuracao": "   "})
        assert r.status_code == 201

        detalhe = client.get(f"/execucoes/{r.json()['execucao_id']}").json()
        assert detalhe["configuracao"] is None
        assert detalhe["observacao"] is None

    def test_recusa_rotulo_longo_demais(self, client, health_check_aprovado):
        r = client.post("/execucoes", json={"tipo_labirinto": "4x4", "configuracao": "x" * 61})
        assert r.status_code == 422

    def test_historico_traz_o_rotulo_e_filtra_por_configuracao(self, client, fabrica):
        a = fabrica.execucao(configuracao="A", iniciada_em=INICIO)
        fabrica.execucao(configuracao="B", iniciada_em=INICIO - timedelta(hours=1))

        lista = client.get("/execucoes", params={"configuracao": "A"})

        assert [i["execucao_id"] for i in lista.json()] == [str(a.execucao_id)]
        assert lista.json()[0]["configuracao"] == "A"
        assert lista.headers["X-Total-Count"] == "1"


class TestCampanha:
    def test_campanha_vazia(self, client):
        corpo = client.get("/execucoes/campanha").json()
        assert corpo["total"] == 0
        assert corpo["taxa_sucesso_pct"] is None
        assert corpo["por_configuracao"] == []

    def test_resumo_conta_sucessos_tempos_e_configuracoes(self, client, fabrica):
        fabrica.execucao(configuracao="A", tempo_total_s=Decimal("120"))
        fabrica.execucao(
            configuracao="A", tempo_total_s=Decimal("620")
        )  # concluída, mas acima de 10 min
        fabrica.execucao(configuracao="A", status="cancelada")
        fabrica.execucao(configuracao="B", tempo_total_s=Decimal("90"))
        fabrica.execucao(status="em_andamento")

        corpo = client.get("/execucoes/campanha").json()

        assert corpo["total"] == 5
        assert corpo["concluidas"] == 3
        assert corpo["canceladas"] == 1
        assert corpo["em_andamento"] == 1
        assert corpo["taxa_sucesso_pct"] == 75.0
        assert corpo["tempo_medio_s"] == 276.67
        assert corpo["tempo_maximo_s"] == 620.0
        assert corpo["concluidas_abaixo_de_10_min"] == 2
        por_nome = {g["configuracao"]: g for g in corpo["por_configuracao"]}
        assert por_nome["A"]["total"] == 3
        assert por_nome["A"]["taxa_sucesso_pct"] == 66.7
        assert por_nome["B"]["taxa_sucesso_pct"] == 100.0
        assert None in por_nome

    def test_campanha_filtra_por_configuracao(self, client, fabrica):
        fabrica.execucao(configuracao="A", tempo_total_s=Decimal("100"))
        fabrica.execucao(configuracao="B", tempo_total_s=Decimal("200"))

        corpo = client.get("/execucoes/campanha", params={"configuracao": "B"}).json()

        assert corpo["total"] == 1
        assert corpo["tempo_maximo_s"] == 200.0


class TestExportacao:
    def _com_falha(self, fabrica):
        execucao = fabrica.execucao(
            configuracao="A", observacao="QA-05", status="cancelada", tentativas_usadas=2
        )
        ok = fabrica.tentativa(execucao, attempt_index=1, status="failed", tempo_s=Decimal("30"))
        fabrica.falha(ok, motivo="stuck", celula=(1, 3))
        fabrica.tentativa(execucao, attempt_index=2, status="success", tempo_s=Decimal("41.5"))
        return execucao

    def test_csv_tem_uma_linha_por_tentativa_e_cabecalho(self, client, fabrica):
        self._com_falha(fabrica)

        r = client.get("/execucoes/exportar")

        assert r.status_code == 200
        assert r.headers["content-type"].startswith("text/csv")
        assert 'filename="execucoes.csv"' in r.headers["content-disposition"]
        linhas = list(csv.DictReader(io.StringIO(r.text)))
        assert [(r["attempt_index"], r["status_tentativa"]) for r in linhas] == [
            ("1", "failed"),
            ("2", "success"),
        ]
        assert linhas[0]["configuracao"] == "A"
        assert linhas[0]["motivo_falha"] == "stuck"
        assert (linhas[0]["celula_x"], linhas[0]["celula_y"]) == ("1", "3")
        assert linhas[1]["motivo_falha"] == ""
        assert linhas[1]["tempo_s"] == "41.5"

    def test_json_traz_os_mesmos_dados(self, client, fabrica):
        self._com_falha(fabrica)

        r = client.get("/execucoes/exportar", params={"formato": "json"})

        assert r.status_code == 200
        assert 'filename="execucoes.json"' in r.headers["content-disposition"]
        corpo = r.json()
        assert len(corpo) == 2
        assert corpo[0]["motivo_falha"] == "stuck"
        assert corpo[1]["motivo_falha"] is None

    def test_exportacao_filtra_e_recusa_formato_invalido(self, client, fabrica):
        self._com_falha(fabrica)
        fabrica.execucao(configuracao="B")

        so_a = client.get("/execucoes/exportar", params={"formato": "json", "configuracao": "A"})
        assert {item["configuracao"] for item in so_a.json()} == {"A"}
        assert client.get("/execucoes/exportar", params={"formato": "xml"}).status_code == 422


class TestVelocidadeArredondada:
    def test_velocidade_media_sai_com_tres_casas_no_historico_e_no_detalhe(self, client, fabrica):
        execucao = fabrica.execucao()
        fabrica.tentativa(execucao, velocidade_media=Decimal("0.4858238949108935"))

        resumo = client.get("/execucoes").json()[0]
        detalhe = client.get(f"/execucoes/{execucao.execucao_id}").json()

        assert resumo["velocidade_media"] == 0.486
        assert detalhe["tentativas"][0]["velocidade_media"] == 0.486

    def test_velocidade_ausente_continua_nula(self, client, fabrica):
        fabrica.tentativa(fabrica.execucao())

        assert client.get("/execucoes").json()[0]["velocidade_media"] is None


class TestNumeroDaExecucao:
    def test_numero_cresce_a_cada_execucao_nova(self, client, fabrica):
        primeira = fabrica.execucao(iniciada_em=INICIO)
        segunda = fabrica.execucao(iniciada_em=INICIO + timedelta(minutes=1))

        assert segunda.numero > primeira.numero >= 1

    def test_historico_e_detalhe_trazem_o_numero(self, client, fabrica):
        execucao = fabrica.execucao()

        resumo = client.get("/execucoes").json()[0]
        detalhe = client.get(f"/execucoes/{execucao.execucao_id}").json()

        assert resumo["numero"] == execucao.numero
        assert detalhe["numero"] == execucao.numero

    def test_execucao_aberta_pela_api_recebe_o_proximo_numero(
        self, client, fabrica, health_check_aprovado
    ):
        anterior = fabrica.execucao(status="cancelada")

        r = client.post("/execucoes", json={"tipo_labirinto": "4x4"})

        detalhe = client.get(f"/execucoes/{r.json()['execucao_id']}").json()
        assert detalhe["numero"] > anterior.numero

    def test_exportacao_traz_o_numero_na_primeira_coluna(self, client, fabrica):
        execucao = fabrica.execucao()
        fabrica.tentativa(execucao)

        linhas = list(csv.DictReader(io.StringIO(client.get("/execucoes/exportar").text)))

        assert list(linhas[0])[0] == "numero"
        assert linhas[0]["numero"] == str(execucao.numero)


def test_estado_do_robo_sem_sinal_vem_nulo(client):
    corpo = client.get("/robo/estado").json()
    assert corpo["ultimo_sinal_em"] is None
    assert corpo["segundos_sem_sinal"] is None


def test_estado_do_robo_informa_o_ultimo_sinal(client):
    client.app.state.monitor_conexao.registrar_mensagem()
    corpo = client.get("/robo/estado").json()
    assert corpo["ultimo_sinal_em"] is not None
    assert 0 <= corpo["segundos_sem_sinal"] < 5
