import json

import pytest
from contrato.telemetria import TAMANHO_MAXIMO, ler_linha
from scripts.carga_telemetria import Registro, gravar_csv, mensagem, sem_crescimento


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


def _tempos(inicial_ms: float, final_ms: float, janela: int = 10) -> list[float]:
    return [inicial_ms / 1000] * janela + [final_ms / 1000] * janela


@pytest.mark.parametrize(
    ("inicial_ms", "final_ms", "esperado"),
    [(3, 7, True), (3, 14, False), (40, 80, True), (40, 81, False)],
)
def test_sem_crescimento_usa_o_dobro_ou_o_piso_de_10_ms(inicial_ms, final_ms, esperado):
    assert sem_crescimento(_tempos(inicial_ms, final_ms), 10) is esperado


def test_sem_crescimento_com_lista_curta_levanta_value_error():
    with pytest.raises(ValueError):
        sem_crescimento([0.001] * 19, 10)


def test_gravar_csv_escreve_cabecalho_e_uma_linha_por_mensagem(tmp_path):
    registros = [
        Registro(0, "tel", 0.0, 0.0031234, 200),
        Registro(1, "passo", 0.1, None, None),
    ]
    caminho = tmp_path / "carga.csv"
    gravar_csv(caminho, registros)
    linhas = caminho.read_text(encoding="utf-8").splitlines()
    assert linhas[0] == "seq,tipo,instante_s,latencia_ms,status"
    assert len(linhas) == 3
    assert linhas[1] == "0,tel,0.000,3.123,200"
    assert linhas[2] == "1,passo,0.100,,"
