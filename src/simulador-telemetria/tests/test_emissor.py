"""Testes do emissor: relógio falso, aceleração, limitador e a saída stdout."""

import json

from contrato.telemetria import ler_linha

from simulador.__main__ import main
from simulador.emissor import Limitador, emitir
from simulador.gerador import LinhaCrua, gerar
from simulador.roteiro import Roteiro

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


class RelogioFalso:
    """Relógio que só anda quando alguém dorme."""

    def __init__(self):
        self.t = 0.0
        self.sonos: list[float] = []

    def agora(self) -> float:
        return self.t

    def dormir(self, segundos: float) -> None:
        self.sonos.append(segundos)
        self.t += segundos


def _emitir(itens, **opcoes):
    relogio = RelogioFalso()
    saida: list[tuple[float, str]] = []
    emitir(itens, lambda linha: saida.append((relogio.agora(), linha)), relogio, **opcoes)
    return relogio, saida


def test_cada_item_sai_no_seu_instante():
    itens = [(0, LinhaCrua("a")), (500, LinhaCrua("b")), (2000, LinhaCrua("c"))]
    _, saida = _emitir(itens)

    assert saida == [(0.0, "a\n"), (0.5, "b\n"), (2.0, "c\n")]


def test_acelerar_divide_esperas_e_t_ms():
    roteiro = Roteiro.model_validate(ROTEIRO)
    normal = list(gerar(roteiro))
    relogio, saida = _emitir(normal, acelerar=4)

    t_final_ms = normal[-1][0]
    assert relogio.agora() == t_final_ms / 1000 / 4
    t_ms = [ler_linha(linha).t_ms for _, linha in saida]
    assert t_ms == [int(item.t_ms / 4) for _, item in normal]


def test_limitador_nunca_passa_de_20_por_segundo():
    relogio = RelogioFalso()
    limitador = Limitador(relogio)
    envios = []
    for _ in range(65):
        limitador.aguardar()
        envios.append(relogio.agora())

    for i in range(len(envios) - 20):
        assert envios[i + 20] - envios[i] >= 1
    assert envios[19] == 0  # as 20 primeiras saem sem esperar
    assert envios[20] == 1


def test_limitador_segura_rajada_acelerada():
    # 100 linhas no mesmo instante simulado: precisam de pelo menos 4 s reais
    itens = [(0, LinhaCrua(str(i))) for i in range(100)]
    relogio, saida = _emitir(itens, acelerar=10)

    assert len(saida) == 100
    assert relogio.agora() >= 4
    instantes = [t for t, _ in saida]
    for i in range(len(instantes) - 20):
        assert instantes[i + 20] - instantes[i] >= 1


def test_sem_espera_nao_dorme():
    itens = [(t, LinhaCrua(str(t))) for t in range(0, 100_000, 10)]
    relogio, saida = _emitir(itens, sem_espera=True)

    assert relogio.sonos == []
    assert len(saida) == len(itens)


def test_main_stdout_sem_espera(tmp_path, capsys):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")

    assert main([str(caminho), "--saida", "stdout", "--sem-espera"]) == 0
    saida = capsys.readouterr()
    linhas = saida.out.splitlines(keepends=True)
    assert len(linhas) == len(list(gerar(Roteiro.model_validate(ROTEIRO))))
    assert all(linha.endswith("\n") for linha in linhas)
    assert [ler_linha(linha).seq for linha in linhas] == list(range(len(linhas)))
    assert "enviada" in saida.err  # o log vai para stderr


def test_taxa_tel_fora_da_faixa_sai_com_1(tmp_path, capsys):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")

    for taxa in ("0.5", "21"):
        assert main([str(caminho), "--sem-espera", "--taxa-tel", taxa]) == 1
    saida = capsys.readouterr()
    assert saida.out == ""
    assert "--taxa-tel" in saida.err


def test_roteiro_invalido_sai_com_1(tmp_path, capsys):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text("{}", encoding="utf-8")

    assert main([str(caminho), "--sem-espera"]) == 1
    assert "roteiro inválido" in capsys.readouterr().err
