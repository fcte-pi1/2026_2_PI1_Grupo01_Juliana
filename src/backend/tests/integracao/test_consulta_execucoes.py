import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

INICIO = datetime(2026, 10, 7, 17, 0, tzinfo=UTC)  # mesmo instante padrão da fábrica


def test_lista_vazia(client):
    response = client.get("/execucoes")
    assert response.status_code == 200
    assert response.json() == []
    assert response.headers["X-Total-Count"] == "0"


def test_lista_em_andamento_primeiro_e_depois_mais_recente(client, fabrica):
    antiga = fabrica.execucao(iniciada_em=INICIO - timedelta(days=2))
    andamento = fabrica.execucao(status="em_andamento", iniciada_em=INICIO - timedelta(days=1))
    recente = fabrica.execucao(tipo="8x4", status="cancelada", iniciada_em=INICIO)

    ids = [item["execucao_id"] for item in client.get("/execucoes").json()]

    assert ids == [str(andamento.execucao_id), str(recente.execucao_id), str(antiga.execucao_id)]


def test_lista_filtra_por_tipo_de_labirinto(client, fabrica):
    fabrica.execucao(tipo="4x4")
    doze = fabrica.execucao(tipo="12x4", iniciada_em=INICIO - timedelta(hours=1))

    response = client.get("/execucoes", params={"tipo_labirinto": "12x4"})

    assert [item["execucao_id"] for item in response.json()] == [str(doze.execucao_id)]
    assert response.headers["X-Total-Count"] == "1"


def test_lista_paginada_com_total(client, fabrica):
    execucoes = [fabrica.execucao(iniciada_em=INICIO - timedelta(hours=h)) for h in range(5)]

    pagina = client.get("/execucoes", params={"limite": 2, "offset": 2})

    assert pagina.status_code == 200
    assert pagina.headers["X-Total-Count"] == "5"
    assert [item["execucao_id"] for item in pagina.json()] == [
        str(execucoes[2].execucao_id),
        str(execucoes[3].execucao_id),
    ]


def test_lista_recusa_limite_fora_da_faixa(client):
    assert client.get("/execucoes", params={"limite": 0}).status_code == 422
    assert client.get("/execucoes", params={"limite": 101}).status_code == 422
    assert client.get("/execucoes", params={"offset": -1}).status_code == 422
    assert client.get("/execucoes", params={"tipo_labirinto": "5x5"}).status_code == 422


def test_resumo_usa_ultima_tentativa_e_ultima_concluida(client, fabrica):
    execucao = fabrica.execucao(
        status="em_andamento", tentativas_usadas=2, tempo_total_s=Decimal("50.0")
    )
    fabrica.tentativa(
        execucao,
        attempt_index=1,
        status="failed",
        velocidade_media=Decimal("0.15"),
        bateria_inicial=Decimal("8.3"),
        bateria_final=Decimal("8.1"),
    )
    fabrica.tentativa(execucao, attempt_index=2, status="running", velocidade_media=Decimal("0.2"))

    (resumo,) = client.get("/execucoes").json()

    assert resumo["tipo_labirinto"] == "4x4"
    assert resumo["tentativas_usadas"] == 2
    assert resumo["tempo_total_s"] == 50.0
    assert resumo["velocidade_media"] == 0.2
    assert resumo["consumo_bateria"] == 0.2


def test_detalhe_inexistente_retorna_404(client):
    response = client.get(f"/execucoes/{uuid.uuid4()}")
    assert response.status_code == 404
    assert response.json() == {"detail": "Execução não encontrada."}


def test_detalhe_com_tentativas_trajeto_health_check_e_falha(client, fabrica):
    execucao = fabrica.execucao_12x4_com_3_tentativas(
        passos_por_tentativa=6, leituras_por_tentativa=4
    )

    detalhe = client.get(f"/execucoes/{execucao.execucao_id}").json()

    assert detalhe["tipo_labirinto"] == "12x4"
    assert detalhe["tempo_total_s"] == 118.4
    assert [t["attempt_index"] for t in detalhe["tentativas"]] == [1, 2, 3]
    assert [t["status"] for t in detalhe["tentativas"]] == ["failed", "failed", "success"]
    assert all(len(t["health_check"]) == 9 for t in detalhe["tentativas"])
    assert detalhe["tentativas"][0]["falha"]["motivo"] == "collision"
    assert detalhe["tentativas"][0]["falha"]["celula_x"] == 5
    assert detalhe["tentativas"][2]["falha"] is None
    assert detalhe["tentativas"][0]["consumo_bateria"] == 0.1
    assert [p["seq"] for p in detalhe["trajeto"]] == list(range(1, 19))
    assert detalhe["eixo_longo"] == "x"
    assert detalhe["variacao_melhor_tempo_pct"] is None


def test_detalhe_devolve_leituras_so_da_ultima_tentativa(client, fabrica):
    execucao = fabrica.execucao(tentativas_usadas=2)
    primeira = fabrica.tentativa(execucao, attempt_index=1, status="failed")
    segunda = fabrica.tentativa(execucao, attempt_index=2, status="success")
    fabrica.leituras(primeira, 5)
    ultimas = fabrica.leituras(segunda, 3)

    detalhe = client.get(f"/execucoes/{execucao.execucao_id}").json()

    assert [lt["ordem"] for lt in detalhe["leituras"]] == [1, 2, 3]
    assert detalhe["ultima_mensagem_em"].startswith(
        ultimas[-1].recebido_em.strftime("%Y-%m-%dT%H:%M:%S")
    )


def test_detalhe_monta_paredes_mask_a_partir_das_paredes(client, fabrica):
    execucao = fabrica.execucao(tentativas_usadas=1)
    tentativa = fabrica.tentativa(execucao)
    fabrica.passos(tentativa, [(0, 0), (1, 0)], paredes_mask=None)
    fabrica.parede(execucao, 0, 0, sul=True, oeste=True)

    trajeto = client.get(f"/execucoes/{execucao.execucao_id}").json()["trajeto"]

    assert [p["paredes_mask"] for p in trajeto] == [2 | 8, 0]


def test_detalhe_falha_sem_celula_usa_ultimo_passo(client, fabrica):
    execucao = fabrica.execucao(status="cancelada", tentativas_usadas=1)
    tentativa = fabrica.tentativa(execucao, status="failed")
    fabrica.passos(tentativa, [(0, 0), (0, 1)])
    fabrica.falha(tentativa, motivo="link_lost", celula=None)

    falha = client.get(f"/execucoes/{execucao.execucao_id}").json()["tentativas"][0]["falha"]

    assert (falha["celula_x"], falha["celula_y"]) == (0, 1)


def test_eixo_longo_y_e_nulo_no_4x4(client, fabrica):
    oito = fabrica.execucao(tipo="8x4", tentativas_usadas=1)
    fabrica.passos(fabrica.tentativa(oito), [(0, 3), (0, 4)])
    quatro = fabrica.execucao(tentativas_usadas=1, iniciada_em=INICIO - timedelta(hours=1))
    fabrica.passos(fabrica.tentativa(quatro), [(3, 3)])

    assert client.get(f"/execucoes/{oito.execucao_id}").json()["eixo_longo"] == "y"
    assert client.get(f"/execucoes/{quatro.execucao_id}").json()["eixo_longo"] is None


def test_variacao_contra_melhor_tempo_anterior_do_mesmo_labirinto(client, fabrica):
    anterior = fabrica.execucao(tentativas_usadas=1, iniciada_em=INICIO - timedelta(days=1))
    fabrica.tentativa(anterior, tempo_s=Decimal("40"))
    outro_labirinto = fabrica.execucao(
        tipo="8x4", tentativas_usadas=1, iniciada_em=INICIO - timedelta(days=1)
    )
    fabrica.tentativa(outro_labirinto, tempo_s=Decimal("10"))
    atual = fabrica.execucao(tentativas_usadas=1)
    fabrica.tentativa(atual, tempo_s=Decimal("30"))

    assert client.get(f"/execucoes/{atual.execucao_id}").json()["variacao_melhor_tempo_pct"] == (
        -25.0
    )
    assert (
        client.get(f"/execucoes/{anterior.execucao_id}").json()["variacao_melhor_tempo_pct"] is None
    )
