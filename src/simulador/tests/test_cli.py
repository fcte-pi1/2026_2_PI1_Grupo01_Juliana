"""Testes da estrutura do simulador e da linha de comando."""

import json

import pytest

from simulador.__main__ import aplicar_ambiente, criar_parser, main

AMBIENTE = {
    "API_URL": "http://api:8000",
    "SIMULADOR_SAIDA": "pty",
    "SIMULADOR_ACELERAR": "4",
    "SIMULADOR_TAXA_TEL_HZ": "10",
}
ROTEIRO = {
    "nome": "teste",
    "labirinto": "4x4",
    "bateria_inicial_mv": 7900,
    "s_por_celula": 0.8,
    "tentativas": [
        {
            "inicio": "nova",
            "celulas": [{"x": 0, "y": 0, "paredes": 14}, {"x": 0, "y": 1, "paredes": 12}],
            "fim": {"tipo": "sucesso"},
        }
    ],
}


def test_contrato_do_backend_importa():
    from contrato.telemetria import ler_linha

    mensagem = ler_linha(
        '{"v":1,"boot":7,"seq":0,"t_ms":412,"tipo":"hc_item",'
        '"componente":"bateria","aprovado":true,"valor":7840}\n'
    )
    assert mensagem.tipo == "hc_item"


def test_help_mostra_as_opcoes(capsys):
    with pytest.raises(SystemExit) as saida:
        criar_parser().parse_args(["--help"])
    assert saida.value.code == 0
    texto = capsys.readouterr().out
    opcoes = ("roteiro", "--saida", "--url", "--acelerar", "--sem-espera", "--taxa-tel", "--boot")
    for opcao in opcoes:
        assert opcao in texto


def test_saida_desconhecida_e_recusada():
    with pytest.raises(SystemExit) as saida:
        criar_parser().parse_args(["r.json", "--saida", "arquivo"])
    assert saida.value.code == 2


def _args(*opcoes):
    return criar_parser().parse_args(["r.json", *opcoes])


def test_sem_opcao_nem_variavel_usa_o_padrao():
    args = _args()
    aplicar_ambiente(args, {})
    assert (args.saida, args.url, args.acelerar, args.taxa_tel) == ("stdout", None, 1.0, 5.0)


def test_variavel_usada_quando_a_opcao_falta():
    args = _args()
    aplicar_ambiente(args, AMBIENTE)
    assert args.saida == "pty"
    assert args.url == "http://api:8000"
    assert args.acelerar == 4.0
    assert args.taxa_tel == 10.0


def test_opcao_vence_a_variavel():
    args = _args(
        "--saida", "stdout", "--url", "http://outra:9000", "--acelerar", "2", "--taxa-tel", "3"
    )
    aplicar_ambiente(args, AMBIENTE)
    assert args.saida == "stdout"
    assert args.url == "http://outra:9000"
    assert args.acelerar == 2.0
    assert args.taxa_tel == 3.0


@pytest.mark.parametrize(
    ("nome", "valor"),
    [
        ("SIMULADOR_TAXA_TEL_HZ", "30"),
        ("SIMULADOR_TAXA_TEL_HZ", "rápido"),
        ("SIMULADOR_ACELERAR", "0"),
        ("SIMULADOR_SAIDA", "arquivo"),
    ],
)
def test_valor_invalido_no_ambiente_sai_com_1(tmp_path, monkeypatch, capsys, nome, valor):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")
    monkeypatch.setenv(nome, valor)

    assert main([str(caminho), "--sem-espera"]) == 1
    saida = capsys.readouterr()
    assert saida.out == ""
    assert nome in saida.err


def test_main_usa_a_taxa_do_ambiente(tmp_path, monkeypatch, capsys):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")

    def tels(taxa):
        monkeypatch.setenv("SIMULADOR_TAXA_TEL_HZ", taxa)
        assert main([str(caminho), "--sem-espera"]) == 0
        linhas = capsys.readouterr().out.splitlines()
        return sum(json.loads(linha)["tipo"] == "tel" for linha in linhas)

    assert tels("20") > tels("1")
