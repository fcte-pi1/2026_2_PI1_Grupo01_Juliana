"""Testes do gerador: health-check, corrida e fim de cada tentativa."""

from contrato.telemetria import (
    Falha,
    HcItem,
    HcResultado,
    Passo,
    Sucesso,
    Tel,
    escrever_linha,
    ler_linha,
)

from simulador.gerador import COMPONENTES, DURACAO_HC_MS, LinhaCrua, Opcoes, gerar
from simulador.roteiro import Roteiro


def _roteiro(**mudancas) -> Roteiro:
    tentativa = {
        "inicio": "nova",
        "celulas": [{"x": 0, "y": 0, "paredes": 14}, {"x": 0, "y": 1, "paredes": 12}],
        "fim": {"tipo": "sucesso"},
    }
    tentativa |= mudancas.pop("tentativa", {})
    dados = {
        "nome": "teste",
        "labirinto": "4x4",
        "bateria_inicial_mv": 7900,
        "s_por_celula": 0.8,
        "tentativas": [tentativa],
    }
    return Roteiro.model_validate(dados | mudancas)


def _mensagens(roteiro: Roteiro, opcoes: Opcoes = Opcoes()) -> list:
    return [item for _, item in gerar(roteiro, opcoes)]


def test_oito_hc_item_na_ordem_do_contrato():
    itens = [m for m in _mensagens(_roteiro()) if isinstance(m, HcItem)]

    assert [m.componente for m in itens] == list(COMPONENTES)
    assert len(itens) == 8
    assert all(m.aprovado for m in itens)


def test_hc_resultado_aprovado_com_dip_e_inicio():
    mensagens = _mensagens(_roteiro(labirinto="8x4"))
    resultado = [m for m in mensagens if isinstance(m, HcResultado)]

    assert len(resultado) == 1
    assert resultado[0].aprovado
    assert resultado[0].tipo_dip == "8x4"
    assert resultado[0].inicio == "nova"
    # o hc_resultado vem depois dos 8 hc_item
    eventos = [m for m in mensagens if isinstance(m, HcItem | HcResultado)]
    assert eventos[-1] is resultado[0]


def test_componente_reprovado_reprova_o_resultado():
    roteiro = _roteiro(tentativa={"hc_reprovados": ["motor_direito"]})
    mensagens = _mensagens(roteiro)

    reprovados = [m.componente for m in mensagens if isinstance(m, HcItem) and not m.aprovado]
    assert reprovados == ["motor_direito"]
    assert not [m for m in mensagens if isinstance(m, HcResultado)][0].aprovado


def test_labirinto_invalido_reprova_o_resultado():
    mensagens = _mensagens(_roteiro(labirinto="invalido"))
    resultado = [m for m in mensagens if isinstance(m, HcResultado)][0]

    assert resultado.tipo_dip == "invalido"
    assert not resultado.aprovado


def test_inicio_padrao_nova_na_primeira_tentativa():
    mensagens = _mensagens(_roteiro(tentativa={"inicio": None}))

    assert [m for m in mensagens if isinstance(m, HcResultado)][0].inicio == "nova"


def test_tel_a_1_hz_no_health_check():
    tels = [(t, m) for t, m in gerar(_roteiro()) if isinstance(m, Tel) and t < DURACAO_HC_MS]

    assert [t for t, _ in tels] == [0, 1000]
    assert all(m.estado == "health-check" for _, m in tels)
    assert all((m.x, m.y, m.vel_mm_s, m.eixo_longo) == (0, 0, 0, None) for _, m in tels)
    assert tels[0][1].bat_mv == 7900


def test_seq_e_t_ms_crescem_desde_zero():
    linha_do_tempo = list(gerar(_roteiro(), Opcoes(boot=42)))
    mensagens = [m for _, m in linha_do_tempo]

    assert [m.seq for m in mensagens] == list(range(len(mensagens)))
    assert all(m.boot == 42 for m in mensagens)
    t_ms = [m.t_ms for m in mensagens]
    assert t_ms == sorted(t_ms)
    assert [t for t, _ in linha_do_tempo] == t_ms


def test_health_check_reprovado_encerra_a_tentativa():
    roteiro = _roteiro(tentativa={"hc_reprovados": ["bateria"]})

    assert len(_mensagens(roteiro)) == 8 + 1 + 2  # hc_item, hc_resultado e 2 tel


def test_toda_mensagem_passa_pelo_contrato():
    for labirinto in ("4x4", "12x4", "invalido"):
        for mensagem in _mensagens(_roteiro(labirinto=labirinto, tentativa=CORRIDA_8X4)):
            linha = escrever_linha(mensagem)
            assert ler_linha(linha) == mensagem
            assert escrever_linha(ler_linha(linha)) == linha


# Corrida em L: sobe a coluna 0 até y = 4 e vira para L; serve para 8x4 e 12x4.
CORRIDA_8X4 = {
    "celulas": [
        {"x": 0, "y": 0, "paredes": 14},
        {"x": 0, "y": 1, "paredes": 12},
        {"x": 0, "y": 2, "paredes": 12},
        {"x": 0, "y": 3, "paredes": 12},
        {"x": 0, "y": 4, "paredes": 9},
        {"x": 1, "y": 4, "paredes": 3},
    ],
    "fim": {"tipo": "falha", "motivo": "falha_componente", "componente": "tof_direito"},
}

# Roteiro 4x4 curto: (0,0) -> (0,1) -> (1,1) -> (0,1), com uma linha crua em (1,1).
CORRIDA_4X4 = {
    "celulas": [
        {"x": 0, "y": 0, "paredes": 14},
        {"x": 0, "y": 1, "paredes": 8},
        {"x": 1, "y": 1, "paredes": 7},
        {"x": 0, "y": 1, "paredes": 8},
    ],
    "eventos": [{"na_celula": 2, "tipo": "linha_crua", "linha": "isto não é json"}],
    "fim": {"tipo": "sucesso"},
}


def test_corrida_4x4_curta():
    linha_do_tempo = list(gerar(_roteiro(tentativa=CORRIDA_4X4)))
    itens = [item for _, item in linha_do_tempo]
    mensagens = [m for m in itens if not isinstance(m, LinhaCrua)]

    for mensagem in mensagens:
        linha = escrever_linha(mensagem)
        assert escrever_linha(ler_linha(linha)) == linha
    assert [m.seq for m in mensagens] == list(range(len(mensagens)))
    assert [t for t, _ in linha_do_tempo] == sorted(t for t, _ in linha_do_tempo)

    tipos = [m.tipo if not isinstance(m, LinhaCrua) else "crua" for m in itens]
    sem_tel = [t for t in tipos if t != "tel"]
    assert sem_tel == ["hc_item"] * 8 + ["hc_resultado", "passo", "passo", "passo", "crua"] + [
        "passo",
        "sucesso",
    ]
    # 2 tel no health-check, 5 Hz por 2,4 s de corrida (0, 200, ..., 2400) e 3 depois do fim
    assert tipos.count("tel") == 2 + 13 + 3


def test_passo_em_cada_celula_inclusive_revisitas():
    linha_do_tempo = list(gerar(_roteiro(tentativa=CORRIDA_4X4)))
    passos = [(t, m) for t, m in linha_do_tempo if isinstance(m, Passo)]

    assert [(m.x, m.y, m.paredes) for _, m in passos] == [
        (0, 0, 14),
        (0, 1, 8),
        (1, 1, 7),
        (0, 1, 8),
    ]
    assert [t for t, _ in passos] == [2000, 2800, 3600, 4400]  # s_por_celula = 0,8


def test_linha_crua_sai_exatamente_como_no_roteiro():
    linha_do_tempo = list(gerar(_roteiro(tentativa=CORRIDA_4X4)))
    cruas = [(t, m) for t, m in linha_do_tempo if isinstance(m, LinhaCrua)]

    assert cruas == [(3600, LinhaCrua("isto não é json"))]


def test_tel_em_movimento_acompanha_celula_e_rumo():
    tels = [m for t, m in gerar(_roteiro(tentativa=CORRIDA_4X4)) if isinstance(m, Tel)]
    running = [m for m in tels if m.estado == "running"]

    assert [(m.x, m.y, m.rumo) for m in running] == (
        [(0, 0, "N")] * 4 + [(0, 1, "N")] * 4 + [(1, 1, "L")] * 4 + [(0, 1, "O")]
    )
    assert all(m.vel_mm_s > 0 and m.eixo_longo is None for m in running)
    assert [m.bat_mv for m in running] == sorted((m.bat_mv for m in running), reverse=True)
    assert running[-1].bat_mv < 7900


def test_taxa_tel_configuravel():
    def tels_running(taxa):
        mensagens = _mensagens(_roteiro(tentativa=CORRIDA_4X4), Opcoes(taxa_tel_hz=taxa))
        return [m.t_ms for m in mensagens if isinstance(m, Tel) and m.estado == "running"]

    assert tels_running(1) == [2000, 3000, 4000]
    assert len(tels_running(20)) == 2400 // 50 + 1


def test_tel_a_1_hz_depois_do_fim():
    tels = [(t, m) for t, m in gerar(_roteiro(tentativa=CORRIDA_4X4)) if isinstance(m, Tel)]
    finais = [(t, m) for t, m in tels if m.estado == "success"]

    assert [t for t, _ in finais] == [5400, 6400, 7400]
    assert all((m.x, m.y, m.vel_mm_s) == (0, 1, 0) for _, m in finais)


def test_rumo_sul_e_oeste():
    tentativa = {
        "celulas": [
            {"x": 1, "y": 1, "paredes": 0},
            {"x": 1, "y": 0, "paredes": 0},
            {"x": 0, "y": 0, "paredes": 0},
        ],
        "fim": {"tipo": "sucesso"},
    }
    tels = [m for m in _mensagens(_roteiro(tentativa=tentativa)) if isinstance(m, Tel)]

    assert {m.rumo for m in tels if (m.x, m.y) == (1, 0)} == {"S"}
    assert {m.rumo for m in tels if (m.x, m.y) == (0, 0)} == {"O"}


def test_eixo_longo_na_primeira_celula_alem_de_3():
    def eixos(labirinto, tentativa):
        mensagens = _mensagens(_roteiro(labirinto=labirinto, tentativa=tentativa))
        return [(m.y, m.eixo_longo) for m in mensagens if isinstance(m, Tel)]

    for y, eixo in eixos("8x4", CORRIDA_8X4):
        assert eixo == ("y" if y > 3 else None)

    em_x = {
        "celulas": [{"x": x, "y": 0, "paredes": 0} for x in range(6)]
        + [{"x": 5, "y": 1, "paredes": 0}],
        "fim": {"tipo": "sucesso"},
    }
    mensagens = _mensagens(_roteiro(labirinto="12x4", tentativa=em_x))
    tels = [m for m in mensagens if isinstance(m, Tel) and m.estado != "health-check"]
    assert {m.eixo_longo for m in tels if m.x <= 3} == {None}
    assert {m.eixo_longo for m in tels if m.x > 3} == {"x"}  # não muda ao subir em y


def test_fim_falha_automatica_na_ultima_celula():
    mensagens = _mensagens(_roteiro(labirinto="8x4", tentativa=CORRIDA_8X4))
    falhas = [m for m in mensagens if isinstance(m, Falha)]

    assert len(falhas) == 1
    falha = falhas[0]
    assert (falha.origem, falha.motivo, falha.componente) == (
        "automatica",
        "falha_componente",
        "tof_direito",
    )
    assert (falha.x, falha.y) == (1, 4)
    depois = mensagens[mensagens.index(falha) + 1 :]
    assert len(depois) == 3
    assert all(isinstance(m, Tel) and m.estado == "failed" for m in depois)
    assert not [m for m in mensagens if isinstance(m, Sucesso)]


def test_fim_sucesso_na_ultima_celula():
    mensagens = _mensagens(_roteiro(tentativa=CORRIDA_4X4))
    sucessos = [m for m in mensagens if isinstance(m, Sucesso)]

    assert [(m.x, m.y) for m in sucessos] == [(0, 1)]


def test_fim_nenhum_sem_evento_final():
    tentativa = CORRIDA_4X4 | {"fim": {"tipo": "nenhum"}}
    mensagens = _mensagens(_roteiro(tentativa=tentativa))

    assert not [m for m in mensagens if isinstance(m, Falha | Sucesso)]
    tels = [m for m in mensagens if isinstance(m, Tel)]
    assert tels[-1].estado == "running"  # sem tel de success/failed depois
