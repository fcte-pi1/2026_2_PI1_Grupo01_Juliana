import json

from contrato.telemetria import TAMANHO_MAXIMO, ler_linha
from scripts.carga_telemetria import mensagem


def test_seq_par_gera_tel_e_impar_gera_passo():
    assert json.loads(mensagem(7, 0, 0))["tipo"] == "tel"
    assert json.loads(mensagem(7, 1, 100))["tipo"] == "passo"


def test_passo_alterna_y_entre_0_e_1():
    ys = [json.loads(mensagem(7, seq, 0))["y"] for seq in (1, 3, 5, 7)]
    assert ys == [0, 1, 0, 1]


def test_linhas_cabem_em_256_bytes_e_seguem_o_contrato():
    for seq in range(20):
        linha = mensagem(2**32 - 1, seq, 2**31) + "\n"
        assert len(linha.encode("utf-8")) <= TAMANHO_MAXIMO
        ler_linha(linha)
