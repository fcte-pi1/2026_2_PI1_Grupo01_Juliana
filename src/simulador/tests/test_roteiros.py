"""Testes dos roteiros prontos em roteiros/, rodados pela linha de comando com --sem-espera."""

from pathlib import Path

import pytest
from contrato.telemetria import LinhaInvalida, escrever_linha, ler_linha

from simulador.__main__ import main
from simulador.roteiro import carregar

ROTEIROS = Path(__file__).parent.parent / "roteiros"


def _rodar(nome: str, capsys) -> list[str]:
    assert main([str(ROTEIROS / nome), "--saida", "stdout", "--sem-espera"]) == 0
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


@pytest.mark.parametrize("nome", ["sucesso-4x4.json", "falha-componente.json", "malformadas.json"])
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
