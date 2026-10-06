"""Testes do gerador: health-check e cabeçalho de cada tentativa."""

from contrato.telemetria import HcItem, HcResultado, Tel, escrever_linha, ler_linha

from simulador.gerador import COMPONENTES, Opcoes, gerar
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


def test_nove_hc_item_na_ordem_do_contrato():
    itens = [m for m in _mensagens(_roteiro()) if isinstance(m, HcItem)]

    assert [m.componente for m in itens] == list(COMPONENTES)
    assert len(itens) == 9
    assert all(m.aprovado for m in itens)


def test_hc_resultado_aprovado_com_dip_e_inicio():
    mensagens = _mensagens(_roteiro(labirinto="8x4"))
    resultado = [m for m in mensagens if isinstance(m, HcResultado)]

    assert len(resultado) == 1
    assert resultado[0].aprovado
    assert resultado[0].tipo_dip == "8x4"
    assert resultado[0].inicio == "nova"
    # o hc_resultado vem depois dos 9 hc_item
    eventos = [m for m in mensagens if not isinstance(m, Tel)]
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
    tels = [(t, m) for t, m in gerar(_roteiro()) if isinstance(m, Tel)]

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

    assert len(_mensagens(roteiro)) == 9 + 1 + 2  # hc_item, hc_resultado e 2 tel


def test_toda_mensagem_passa_pelo_contrato():
    for labirinto in ("4x4", "12x4", "invalido"):
        for mensagem in _mensagens(_roteiro(labirinto=labirinto)):
            linha = escrever_linha(mensagem)
            assert ler_linha(linha) == mensagem
            assert escrever_linha(ler_linha(linha)) == linha
