"""Testes do modelo e do carregador do roteiro."""

import copy
import json

import pytest

from simulador.roteiro import FimFalha, PerdaLink, RoteiroInvalido, carregar

VALIDO = {
    "nome": "colisao-retomada",
    "labirinto": "4x4",
    "bateria_inicial_mv": 7900,
    "s_por_celula": 0.8,
    "tentativas": [
        {
            "inicio": "nova",
            "hc_reprovados": [],
            "celulas": [
                {"x": 0, "y": 0, "paredes": 14},
                {"x": 0, "y": 1, "paredes": 12},
                {"x": 0, "y": 2, "paredes": 8},
            ],
            "eventos": [
                {"na_celula": 1, "tipo": "perda_link", "segundos": 7},
                {"na_celula": 2, "tipo": "linha_crua", "linha": "isto não é json"},
            ],
            "fim": {"tipo": "falha", "motivo": "collision"},
        },
        {
            "pausa_s": 5,
            "inicio": "retomada",
            "celulas": [{"x": 0, "y": 2, "paredes": 8}, {"x": 1, "y": 2, "paredes": 3}],
            "fim": {"tipo": "sucesso"},
        },
    ],
}


def _gravar(tmp_path, dados) -> str:
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(dados), encoding="utf-8")
    return str(caminho)


def _erro(tmp_path, dados) -> str:
    with pytest.raises(RoteiroInvalido) as erro:
        carregar(_gravar(tmp_path, dados))
    return str(erro.value)


def test_roteiro_valido(tmp_path):
    roteiro = carregar(_gravar(tmp_path, VALIDO))

    assert roteiro.labirinto == "4x4"
    primeira, segunda = roteiro.tentativas
    assert isinstance(primeira.eventos[0], PerdaLink)
    assert primeira.eventos[1].linha == "isto não é json"
    assert isinstance(primeira.fim, FimFalha)
    assert primeira.fim.componente is None
    assert segunda.pausa_s == 5
    assert segunda.hc_reprovados == []
    assert segunda.eventos == []


def test_inicio_e_opcional(tmp_path):
    dados = copy.deepcopy(VALIDO)
    del dados["tentativas"][1]["inicio"]

    assert carregar(_gravar(tmp_path, dados)).tentativas[1].inicio is None


def test_erro_traz_o_caminho_do_campo(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celulas"][2]["x"] = 12

    assert "tentativas[0].celulas[2].x:" in _erro(tmp_path, dados)


def test_caminho_ignora_o_rotulo_da_uniao(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["eventos"][0]["segundos"] = -1

    assert "tentativas[0].eventos[0].segundos:" in _erro(tmp_path, dados)


def test_rejeita_celulas_que_nao_sao_vizinhas(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celulas"][2] = {"x": 1, "y": 2, "paredes": 8}

    mensagem = _erro(tmp_path, dados)
    assert "tentativas[0]:" in mensagem
    assert "celulas[2] (1, 2) não é vizinha de celulas[1] (0, 1)" in mensagem


def test_rejeita_celula_repetida_em_sequencia(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][1]["celulas"][1] = {"x": 0, "y": 2, "paredes": 8}

    assert "não é vizinha" in _erro(tmp_path, dados)


def test_aceita_revisita(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celulas"].append({"x": 0, "y": 1, "paredes": 12})

    assert len(carregar(_gravar(tmp_path, dados)).tentativas[0].celulas) == 4


def test_rejeita_evento_fora_da_lista_de_celulas(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["eventos"][1]["na_celula"] = 3

    mensagem = _erro(tmp_path, dados)
    assert "tentativas[0]:" in mensagem
    assert "eventos[1].na_celula = 3 fora da lista de 3 células" in mensagem


def test_rejeita_tipo_de_evento_desconhecido(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["eventos"][0] = {"na_celula": 0, "tipo": "explosao"}

    assert "tentativas[0].eventos[0]:" in _erro(tmp_path, dados)


@pytest.mark.parametrize(
    "fim",
    [
        {"tipo": "falha", "motivo": "falha_componente"},
        {"tipo": "falha", "motivo": "collision", "componente": "motor_esquerdo"},
        {"tipo": "falha", "motivo": "encerrado_operador"},
    ],
)
def test_rejeita_fim_falha_incoerente(tmp_path, fim):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["fim"] = fim

    assert "tentativas[0].fim" in _erro(tmp_path, dados)


def test_aceita_falha_componente_com_componente(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["fim"] = {
        "tipo": "falha",
        "motivo": "falha_componente",
        "componente": "motor_esquerdo",
    }

    assert carregar(_gravar(tmp_path, dados)).tentativas[0].fim.componente == "motor_esquerdo"


def test_rejeita_labirinto_desconhecido(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["labirinto"] = "16x16"

    assert "labirinto:" in _erro(tmp_path, dados)


def test_rejeita_campo_desconhecido(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celula"] = []

    assert "tentativas[0].celula:" in _erro(tmp_path, dados)


def test_json_quebrado_e_arquivo_ausente(tmp_path):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text("{nada", encoding="utf-8")
    with pytest.raises(RoteiroInvalido, match="roteiro.json"):
        carregar(caminho)

    with pytest.raises(RoteiroInvalido, match="não foi possível ler"):
        carregar(tmp_path / "nao-existe.json")


def test_mensagens_em_portugues(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["labirinto"] = "16x16"
    dados["tentativas"][0]["celulas"][2]["x"] = 12
    del dados["tentativas"][0]["eventos"][0]["segundos"]

    mensagem = _erro(tmp_path, dados)
    assert "labirinto: deve ser '4x4', '8x4', '12x4' ou 'invalido'" in mensagem
    assert "tentativas[0].celulas[2].x: deve ser no máximo 11" in mensagem
    assert "tentativas[0].eventos[0].segundos: campo obrigatório" in mensagem


def test_rejeita_travessia_pela_parede_de_saida(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celulas"][0]["paredes"] = 15  # (0, 0) com parede N

    mensagem = _erro(tmp_path, dados)
    assert "tentativas[0].celulas[1]: atravessa a parede N de (0, 0)" in mensagem
    assert "(raiz)" not in mensagem


def test_rejeita_travessia_pela_parede_de_entrada(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][1]["celulas"][1]["paredes"] = 11  # (1, 2) com parede O

    assert "tentativas[1].celulas[1]: atravessa a parede O de (1, 2)" in _erro(tmp_path, dados)


def test_rejeita_revisita_com_mascara_diferente(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][1]["celulas"][0]["paredes"] = 9  # era 8 na tentativa 0

    mensagem = _erro(tmp_path, dados)
    assert "tentativas[1].celulas[0]: (0, 2) tem paredes 9" in mensagem
    assert "mas tentativas[0].celulas[2] tem 8" in mensagem


# De (1, 2) até ao lado de (1, 1) sem passar pela parede S de (1, 2).
_CONTORNO = [{"x": 2, "y": 2, "paredes": 0}, {"x": 2, "y": 1, "paredes": 0}]


def test_rejeita_vizinhas_que_discordam_da_parede(tmp_path):
    dados = copy.deepcopy(VALIDO)
    # O robô não passa entre (0, 1), que tem parede L, e (1, 1), que não tem parede O.
    dados["tentativas"][1]["celulas"] += _CONTORNO + [{"x": 1, "y": 1, "paredes": 1}]

    assert (
        "tentativas[0].celulas[1] (0, 1) e tentativas[1].celulas[4] (1, 1) "
        "discordam na parede entre elas"
    ) in _erro(tmp_path, dados)


def test_aceita_revisita_coerente_entre_tentativas(tmp_path):
    dados = copy.deepcopy(VALIDO)
    dados["tentativas"][0]["celulas"].append({"x": 0, "y": 1, "paredes": 12})
    dados["tentativas"][1]["celulas"] += _CONTORNO + [{"x": 1, "y": 1, "paredes": 9}]

    roteiro = carregar(_gravar(tmp_path, dados))
    assert roteiro.tentativas[1].celulas[0] == roteiro.tentativas[0].celulas[2]
