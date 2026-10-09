"""Testes dos roteiros prontos em roteiros/, rodados pela linha de comando com --sem-espera."""

from pathlib import Path

import pytest
from contrato.telemetria import LinhaInvalida, escrever_linha, ler_linha

from simulador.__main__ import main
from simulador.roteiro import carregar

ROTEIROS = Path(__file__).parent.parent / "roteiros"


def _rodar(nome: str, capsys) -> list[str]:
    assert main([str(ROTEIROS / nome), "--saida", "stdout", "--sem-espera", "--boot", "0"]) == 0
    return capsys.readouterr().out.splitlines(keepends=True)


def _crus(nome: str) -> set[str]:
    """Linhas cruas do roteiro, que saem como estão (com o \\n do emissor)."""
    roteiro = carregar(ROTEIROS / nome)
    return {
        evento.linha + "\n"
        for tentativa in roteiro.tentativas
        for evento in tentativa.eventos
        if evento.tipo == "linha_crua"
    }


@pytest.mark.parametrize("nome", sorted(p.name for p in ROTEIROS.glob("*.json")))
def test_linhas_validas_passam_e_malformadas_sao_recusadas(nome, capsys):
    crus = _crus(nome)
    validas = []
    for linha in _rodar(nome, capsys):
        if linha in crus:
            with pytest.raises(LinhaInvalida):
                ler_linha(linha)
        else:
            mensagem = ler_linha(linha)
            assert escrever_linha(mensagem) == linha
            validas.append(mensagem)
    assert validas


def test_sucesso_4x4_chega_ao_centro(capsys):
    mensagens = [ler_linha(linha) for linha in _rodar("sucesso-4x4.json", capsys)]
    resultado = next(m for m in mensagens if m.tipo == "hc_resultado")
    assert resultado.aprovado and resultado.inicio == "nova"
    sucesso = next(m for m in mensagens if m.tipo == "sucesso")
    assert (sucesso.x, sucesso.y) in {(1, 1), (1, 2), (2, 1), (2, 2)}


def test_falha_componente_no_meio_da_corrida(capsys):
    mensagens = [ler_linha(linha) for linha in _rodar("falha-componente.json", capsys)]
    falha = next(m for m in mensagens if m.tipo == "falha")
    assert falha.origem == "automatica"
    assert falha.motivo == "falha_componente"
    assert falha.componente is not None
    assert not any(m.tipo == "sucesso" for m in mensagens)


def test_malformadas_tem_um_caso_de_cada(capsys):
    linhas = _rodar("malformadas.json", capsys)
    crus = _crus("malformadas.json")
    assert len(crus) == 6
    assert all(linha in linhas for linha in crus)
    assert any(len(linha.encode()) > 256 for linha in crus)
    assert sum(1 for linha in linhas if linha not in crus) > len(crus)


def test_todos_os_roteiros_estao_cobertos():
    nomes = {p.stem for p in ROTEIROS.glob("*.json")}
    assert nomes >= {
        "sucesso-4x4",
        "colisao-retomada",
        "falha-componente",
        "perda-link",
        "reconexao",
        "malformadas",
        "estouro-10min",
    }


def test_colisao_e_depois_retomada_com_sucesso(capsys):
    mensagens = [ler_linha(linha) for linha in _rodar("colisao-retomada.json", capsys)]
    assert sorted({m.boot for m in mensagens}) == [0, 1]
    primeira = [m for m in mensagens if m.boot == 0]
    segunda = [m for m in mensagens if m.boot == 1]
    falha = next(m for m in primeira if m.tipo == "falha")
    assert (falha.motivo, falha.origem) == ("collision", "automatica")
    assert not any(m.tipo == "sucesso" for m in primeira)
    resultado = next(m for m in segunda if m.tipo == "hc_resultado")
    assert resultado.aprovado and resultado.inicio == "retomada"
    assert any(m.tipo == "sucesso" for m in segunda)


def _maior_silencio_ms(mensagens) -> int:
    """Maior intervalo de t_ms entre tel seguidas, na ordem em que saíram."""
    tempos = [m.t_ms for m in mensagens if m.tipo == "tel"]
    return max(b - a for a, b in zip(tempos, tempos[1:]))


def _reenviado(mensagens) -> bool:
    seqs = [m.seq for m in mensagens]
    return any(b < a for a, b in zip(seqs, seqs[1:]))


@pytest.mark.parametrize(("nome", "segundos"), [("perda-link.json", 12), ("reconexao.json", 7)])
def test_queda_no_meio_da_corrida_e_reenvio(nome, segundos, capsys):
    mensagens = [ler_linha(linha) for linha in _rodar(nome, capsys)]
    passos = [m for m in mensagens if m.tipo == "passo"]
    assert _maior_silencio_ms(mensagens) >= segundos * 1000
    silencio_fim = max(
        b.t_ms for a, b in zip(mensagens, mensagens[1:]) if b.t_ms - a.t_ms >= segundos * 1000
    )
    assert silencio_fim < passos[-1].t_ms  # a corrida continua depois da volta do link
    assert _reenviado(mensagens)
    # O buffer volta inteiro, inclusive o que saiu antes da queda: o backend tira pelo seq.
    eventos = [m for m in mensagens if m.tipo != "tel"]
    assert len({m.seq for m in eventos}) < len(eventos)
    assert mensagens[-1].tipo == "tel"
    assert any(m.tipo == "sucesso" for m in mensagens)


def test_estouro_anda_mais_de_10_min_sem_fim(capsys):
    mensagens = [ler_linha(linha) for linha in _rodar("estouro-10min.json", capsys)]
    assert not any(m.tipo in {"sucesso", "falha"} for m in mensagens)
    correndo = [m for m in mensagens if m.tipo == "tel" and m.estado == "running"]
    assert correndo[-1].t_ms - correndo[0].t_ms > 600_000
    assert mensagens[-1].tipo in {"tel", "passo"}
