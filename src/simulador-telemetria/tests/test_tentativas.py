"""Testes de várias tentativas: boots, pausa, boot persistido e parada pelo BOOT."""

import json

from contrato.telemetria import Deduplicador, Falha, HcResultado, Passo, Tel, ler_linha

from simulador.__main__ import main
from simulador.gerador import DURACAO_FINAL_MS, DURACAO_HC_MS, LinhaCrua, Opcoes, gerar
from simulador.roteiro import Roteiro

S_POR_CELULA = 0.8
CELULAS = [
    {"x": 0, "y": 0, "paredes": 10},
    {"x": 0, "y": 1, "paredes": 8},
    {"x": 0, "y": 2, "paredes": 8},
    {"x": 0, "y": 3, "paredes": 9},
]


def _dados(*tentativas: dict) -> dict:
    tentativas = tentativas or (
        {"celulas": CELULAS[:2], "fim": {"tipo": "falha", "motivo": "collision"}},
        {"pausa_s": 5, "celulas": CELULAS[1:], "fim": {"tipo": "sucesso"}},
    )
    return {
        "nome": "teste",
        "labirinto": "4x4",
        "bateria_inicial_mv": 7900,
        "s_por_celula": S_POR_CELULA,
        "tentativas": list(tentativas),
    }


def _por_boot(itens) -> dict[int, list]:
    boots: dict[int, list] = {}
    for t, item in itens:
        if not isinstance(item, LinhaCrua):
            boots.setdefault(item.boot, []).append((t, item))
    return boots


def test_cada_tentativa_e_um_boot_novo():
    boots = _por_boot(gerar(Roteiro.model_validate(_dados()), Opcoes(boot=7)))

    assert list(boots) == [7, 8]
    for itens in boots.values():
        assert [m.seq for _, m in itens] == list(range(len(itens)))
        assert itens[0][1].t_ms == 0


def test_pausa_sem_nenhuma_linha_entre_tentativas():
    itens = list(gerar(Roteiro.model_validate(_dados())))
    boots = _por_boot(itens)
    fim_primeira = boots[0][-1][0]
    inicio_segunda = boots[1][0][0]

    assert inicio_segunda - fim_primeira >= 5000
    assert not [t for t, _ in itens if fim_primeira < t < inicio_segunda]


def test_inicio_nova_e_depois_retomada_salvo_o_roteiro():
    def inicios(dados):
        itens = gerar(Roteiro.model_validate(dados))
        return [m.inicio for _, m in itens if isinstance(m, HcResultado)]

    assert inicios(_dados()) == ["nova", "retomada"]
    dados = _dados()
    dados["tentativas"][1]["inicio"] = "nova"
    assert inicios(dados) == ["nova", "nova"]


def test_parada_boot_gera_falha_e_encerra_a_tentativa():
    tentativa = {
        "celulas": CELULAS,
        "eventos": [
            {"na_celula": 1, "tipo": "parada_boot"},
            {"na_celula": 3, "tipo": "linha_crua", "linha": "não sai"},
        ],
        "fim": {"tipo": "sucesso"},
    }
    itens = list(gerar(Roteiro.model_validate(_dados(tentativa))))
    mensagens = [m for _, m in itens]
    falhas = [m for m in mensagens if isinstance(m, Falha)]
    passos = [m for m in mensagens if isinstance(m, Passo)]
    fim_ms = DURACAO_HC_MS + round(S_POR_CELULA * 1000)

    assert len(falhas) == 1
    assert (falhas[0].origem, falhas[0].motivo) == ("boot", "encerrado_operador")
    assert (falhas[0].x, falhas[0].y, falhas[0].componente) == (0, 1, None)
    assert falhas[0].t_ms == fim_ms
    assert [(p.x, p.y) for p in passos] == [(0, 0), (0, 1)]
    assert not any(isinstance(m, LinhaCrua) for m in mensagens)
    depois = [m for m in mensagens if m.seq > falhas[0].seq]
    assert all(isinstance(m, Tel) and m.estado == "failed" for m in depois)
    assert itens[-1][0] == fim_ms + DURACAO_FINAL_MS


def test_parada_boot_corta_a_tentativa_e_segue_para_a_proxima():
    primeira = {
        "celulas": CELULAS,
        "eventos": [{"na_celula": 2, "tipo": "parada_boot"}],
        "fim": {"tipo": "nenhum"},
    }
    segunda = {"pausa_s": 2, "celulas": CELULAS[2:], "fim": {"tipo": "sucesso"}}
    boots = _por_boot(gerar(Roteiro.model_validate(_dados(primeira, segunda))))

    assert [m.tipo for _, m in boots[0] if not isinstance(m, Tel)][-1] == "falha"
    assert [m.tipo for _, m in boots[1] if not isinstance(m, Tel)][-1] == "sucesso"


def test_parada_boot_durante_queda_sai_no_reenvio():
    tentativa = {
        "celulas": CELULAS,
        "eventos": [
            {"na_celula": 1, "tipo": "perda_link", "segundos": 2},
            {"na_celula": 2, "tipo": "parada_boot"},
            {"na_celula": 3, "tipo": "perda_link", "segundos": 30},
        ],
        "fim": {"tipo": "sucesso"},
    }
    itens = list(gerar(Roteiro.model_validate(_dados(tentativa))))
    volta_ms = DURACAO_HC_MS + round(S_POR_CELULA * 1000) + 2000
    falha = [(t, m) for t, m in itens if isinstance(m, Falha)]

    assert len(falha) == 1
    assert falha[0][0] >= volta_ms  # parou sem link: a falha sai depois da volta
    assert itens[-1][0] < volta_ms + 30_000  # a queda depois da parada não acontece


def _rodar(tmp_path, capsys, *opcoes) -> list:
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(_dados()))
    assert main([str(caminho), "--sem-espera", *opcoes]) == 0
    return [ler_linha(linha + "\n") for linha in capsys.readouterr().out.splitlines()]


def test_duas_execucoes_nao_repetem_boot(tmp_path, capsys, arquivo_boot):
    primeira = _rodar(tmp_path, capsys)
    segunda = _rodar(tmp_path, capsys)

    assert sorted({m.boot for m in primeira}) == [0, 1]
    assert sorted({m.boot for m in segunda}) == [2, 3]
    assert arquivo_boot.read_text().strip() == "4"

    dedup = Deduplicador()
    for mensagem in primeira:
        dedup.eh_nova(mensagem)
    assert all(dedup.eh_nova(m) for m in segunda)


def test_boot_forcado_vence_o_salvo(tmp_path, capsys, arquivo_boot):
    _rodar(tmp_path, capsys)
    mensagens = _rodar(tmp_path, capsys, "--boot", "40")

    assert sorted({m.boot for m in mensagens}) == [40, 41]
    assert arquivo_boot.read_text().strip() == "42"


def test_boot_negativo_e_arquivo_estragado_saem_com_1(tmp_path, capsys, arquivo_boot):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(_dados()))

    assert main([str(caminho), "--sem-espera", "--boot", "-1"]) == 1
    arquivo_boot.parent.mkdir(parents=True)
    arquivo_boot.write_text("abc")
    assert main([str(caminho), "--sem-espera"]) == 1
    assert "boot" in capsys.readouterr().err
