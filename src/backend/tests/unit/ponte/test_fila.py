import threading
from datetime import datetime, timedelta, timezone

import pytest
from ponte.fila import Fila, classificar

FUSO = timezone(timedelta(hours=-3))
INICIO = datetime(2026, 10, 21, 9, 0, 0, tzinfo=FUSO)

TEL = '{"v":1,"tipo":"tel","t":%d}'
EVENTO = '{"v":1,"tipo":"inicio","t":%d}'


def item(linha, segundos=0):
    return (linha, INICIO + timedelta(seconds=segundos))


def esvaziar(fila):
    linhas = []
    while (atual := fila.primeiro(timeout=0)) is not None:
        linhas.append(atual[0])
        assert fila.remover_primeiro()
    return linhas


@pytest.mark.parametrize(
    ("linha", "classe"),
    [
        ('{"v":1,"tipo":"tel"}', "tel"),
        ('{"v":1,"tipo":"inicio"}', "evento"),
        ('{"v":1,"tipo":"fim_tentativa"}', "evento"),
        ("isto não é JSON", "invalida"),
        ('{"v":1}', "invalida"),
        ('{"v":1,"tipo":3}', "invalida"),
        ('["tel"]', "invalida"),
        ("", "invalida"),
    ],
)
def test_classificar(linha, classe):
    assert classificar(linha) == classe


def test_maximo_menor_que_1_e_recusado():
    with pytest.raises(ValueError):
        Fila(0)


def test_ordem_fifo():
    fila = Fila(10)
    for i in range(5):
        assert fila.colocar(item(TEL % i, i)) is None
    assert len(fila) == 5
    assert esvaziar(fila) == [TEL % i for i in range(5)]
    assert len(fila) == 0


def test_primeiro_nao_remove():
    fila = Fila(10)
    fila.colocar(item(TEL % 1))
    assert fila.primeiro(timeout=0) == item(TEL % 1)
    assert fila.primeiro(timeout=0) == item(TEL % 1)
    assert len(fila) == 1


def test_primeiro_estoura_o_timeout_com_a_fila_vazia():
    assert Fila(10).primeiro(timeout=0.01) is None


def test_primeiro_bloqueia_ate_chegar_item():
    fila = Fila(10)
    resultado = []
    leitor = threading.Thread(target=lambda: resultado.append(fila.primeiro(timeout=2)))
    leitor.start()
    fila.colocar(item(TEL % 1))
    leitor.join(timeout=2)
    assert resultado == [item(TEL % 1)]


def test_descarta_primeiro_a_invalida_mais_antiga():
    fila = Fila(4)
    fila.colocar(item(EVENTO % 0, 0))
    fila.colocar(item("lixo 1", 1))
    fila.colocar(item(TEL % 2, 2))
    fila.colocar(item("lixo 2", 3))
    assert fila.colocar(item(TEL % 4, 4)) == item("lixo 1", 1)
    assert esvaziar(fila) == [EVENTO % 0, TEL % 2, "lixo 2", TEL % 4]


def test_sem_invalida_descarta_a_tel_mais_antiga():
    fila = Fila(4)
    fila.colocar(item(EVENTO % 0, 0))
    fila.colocar(item(TEL % 1, 1))
    fila.colocar(item(EVENTO % 2, 2))
    fila.colocar(item(TEL % 3, 3))
    assert fila.colocar(item(EVENTO % 4, 4)) == item(TEL % 1, 1)
    assert esvaziar(fila) == [EVENTO % 0, EVENTO % 2, TEL % 3, EVENTO % 4]


def test_so_eventos_descarta_o_evento_mais_antigo():
    fila = Fila(3)
    for i in range(3):
        fila.colocar(item(EVENTO % i, i))
    assert fila.colocar(item(EVENTO % 3, 3)) == item(EVENTO % 0, 0)
    assert esvaziar(fila) == [EVENTO % 1, EVENTO % 2, EVENTO % 3]


def test_json_sem_tipo_e_descartado_antes_da_tel():
    fila = Fila(2)
    fila.colocar(item(TEL % 0, 0))
    fila.colocar(item('{"v":1}', 1))
    assert fila.colocar(item(TEL % 2, 2)) == item('{"v":1}', 1)
    assert esvaziar(fila) == [TEL % 0, TEL % 2]


def test_maximo_1():
    fila = Fila(1)
    assert fila.colocar(item(TEL % 0, 0)) is None
    assert fila.colocar(item(TEL % 1, 1)) == item(TEL % 0, 0)
    assert len(fila) == 1
    assert esvaziar(fila) == [TEL % 1]


def test_descarte_do_item_em_reenvio_nao_remove_outro():
    fila = Fila(2)
    fila.colocar(item(TEL % 0, 0))
    fila.colocar(item(EVENTO % 1, 1))
    assert fila.primeiro(timeout=0) == item(TEL % 0, 0)
    # Enquanto a enviadora reenvia a tel, a fila enche e a tel é descartada.
    assert fila.colocar(item(EVENTO % 2, 2)) == item(TEL % 0, 0)
    assert fila.remover_primeiro() is False
    assert esvaziar(fila) == [EVENTO % 1, EVENTO % 2]


def test_maximo_1_com_o_unico_item_em_reenvio():
    fila = Fila(1)
    fila.colocar(item(TEL % 0, 0))
    assert fila.primeiro(timeout=0) == item(TEL % 0, 0)
    fila.colocar(item(TEL % 1, 1))
    assert fila.remover_primeiro() is False
    assert esvaziar(fila) == [TEL % 1]


def test_linhas_iguais_sao_itens_distintos():
    fila = Fila(2)
    fila.colocar(item(TEL % 0, 0))
    assert fila.primeiro(timeout=0) == item(TEL % 0, 0)
    fila.colocar(item(TEL % 0, 0))
    fila.colocar(item(TEL % 0, 0))
    # O primeiro (em reenvio) foi descartado; o igual que ficou no início não sai.
    assert fila.remover_primeiro() is False
    assert len(fila) == 2


def test_remover_primeiro_sem_primeiro_nao_remove():
    fila = Fila(2)
    fila.colocar(item(TEL % 0, 0))
    assert fila.remover_primeiro() is False
    assert len(fila) == 1
