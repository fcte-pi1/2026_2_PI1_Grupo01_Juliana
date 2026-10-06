"""Testes da estrutura do simulador e da linha de comando."""

import pytest

from simulador.__main__ import criar_parser


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
