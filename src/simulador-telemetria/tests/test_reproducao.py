"""Testes da reprodução de um .jsonl pronto, como o gerado pelo simulador de navegação."""

import json
from pathlib import Path

import pytest
from contrato.telemetria import Comando, EntradaPonte, RespostaPonte, ler_linha
from fastapi import FastAPI
from fastapi.testclient import TestClient

import simulador.__main__ as cli
from simulador.__main__ import main
from simulador.reproducao import carregar_gravacao, reproduzir
from simulador.roteiro import RoteiroInvalido

FIRM02 = Path(__file__).parent / "dados" / "firm02-4x4-01-dir.jsonl"


def _linha(boot: int, seq: int, t_ms: int) -> str:
    return json.dumps(
        {
            "v": 1,
            "boot": boot,
            "seq": seq,
            "t_ms": t_ms,
            "tipo": "tel",
            "estado": "running",
            "x": 0,
            "y": 0,
            "rumo": "N",
            "bat_mv": 8000,
            "vel_mm_s": 0,
            "eixo_longo": None,
        }
    )


def _gravar(tmp_path, *linhas: str) -> Path:
    caminho = tmp_path / "gravacao.jsonl"
    caminho.write_text("".join(linha + "\n" for linha in linhas), encoding="utf-8")
    return caminho


def test_carrega_o_jsonl_do_firm02():
    gravacao = carregar_gravacao(FIRM02)
    assert gravacao.nome == "firm02-4x4-01-dir"
    assert gravacao.boots == 1
    assert [m.tipo for m in gravacao.mensagens][-3:] == ["passo", "falha", "tel"]


def test_reescreve_o_boot_e_mantem_o_resto():
    gravacao = carregar_gravacao(FIRM02)
    reproduzidas = [m for _, m in reproduzir(gravacao, primeiro_boot=7)]
    assert {m.boot for m in reproduzidas} == {7}
    originais = [m.model_dump(exclude={"boot"}) for m in gravacao.mensagens]
    assert [m.model_dump(exclude={"boot"}) for m in reproduzidas] == originais


def test_instante_vem_do_t_ms():
    gravacao = carregar_gravacao(FIRM02)
    instantes = [t for t, _ in reproduzir(gravacao, primeiro_boot=0)]
    assert instantes == [m.t_ms for m in gravacao.mensagens]


def test_cada_boot_do_arquivo_vira_um_boot_novo_em_sequencia(tmp_path):
    caminho = _gravar(
        tmp_path, _linha(1, 0, 100), _linha(1, 1, 900), _linha(5, 0, 200), _linha(5, 1, 400)
    )
    gravacao = carregar_gravacao(caminho)
    assert gravacao.boots == 2
    itens = list(reproduzir(gravacao, primeiro_boot=10))
    assert [m.boot for _, m in itens] == [10, 10, 11, 11]
    # o t_ms recomeça no boot novo; o instante continua de onde o boot anterior parou
    assert [t for t, _ in itens] == [100, 900, 1100, 1300]


def test_instante_nunca_volta(tmp_path):
    # reenvio do buffer depois de uma queda: evento antigo sai depois, com t_ms menor
    caminho = _gravar(tmp_path, _linha(1, 0, 100), _linha(1, 2, 800), _linha(1, 1, 300))
    instantes = [t for t, _ in reproduzir(carregar_gravacao(caminho), primeiro_boot=0)]
    assert instantes == [100, 800, 800]


def test_linha_fora_do_contrato_e_recusada_com_o_numero(tmp_path):
    caminho = _gravar(tmp_path, _linha(1, 0, 100), '{"v":2}')
    with pytest.raises(RoteiroInvalido, match="linha 2"):
        carregar_gravacao(caminho)


def test_linhas_em_branco_sao_ignoradas(tmp_path):
    caminho = _gravar(tmp_path, _linha(1, 0, 100), "", _linha(1, 1, 200))
    assert len(carregar_gravacao(caminho).mensagens) == 2


def test_arquivo_vazio_e_recusado(tmp_path):
    with pytest.raises(RoteiroInvalido, match="nenhuma mensagem"):
        carregar_gravacao(_gravar(tmp_path))


def test_arquivo_inexistente_e_recusado(tmp_path):
    with pytest.raises(RoteiroInvalido, match="não foi possível ler"):
        carregar_gravacao(tmp_path / "nao-existe.jsonl")


def test_main_reproduz_no_stdout(capsys):
    assert main([str(FIRM02), "--sem-espera", "--boot", "3"]) == 0
    saida = [ler_linha(linha) for linha in capsys.readouterr().out.splitlines()]
    originais = carregar_gravacao(FIRM02).mensagens
    assert [m.model_copy(update={"boot": 3}) for m in originais] == saida


def test_duas_reproducoes_seguidas_usam_boots_diferentes(capsys, arquivo_boot):
    def boots() -> set[int]:
        assert main([str(FIRM02), "--sem-espera"]) == 0
        return {json.loads(linha)["boot"] for linha in capsys.readouterr().out.splitlines()}

    primeira, segunda = boots(), boots()
    assert primeira == {0}
    assert segunda == {1}
    assert arquivo_boot.read_text() == "2\n"


def test_main_reproduz_pelo_http(capsys, monkeypatch):
    assert main([str(FIRM02), "--sem-espera", "--boot", "0"]) == 0
    esperadas = capsys.readouterr().out.splitlines()

    recebidas: list[str] = []
    app = FastAPI()

    @app.post("/telemetria")
    def telemetria(entrada: EntradaPonte) -> RespostaPonte:
        recebidas.append(entrada.linha)
        return RespostaPonte()

    monkeypatch.setattr(cli, "criar_cliente", lambda url: TestClient(app))
    argumentos = [str(FIRM02), "--saida", "http", "--url", "http://api", "--sem-espera"]
    assert main([*argumentos, "--boot", "0"]) == 0
    assert recebidas == esperadas


def test_main_jsonl_invalido_sai_com_1(tmp_path, capsys):
    caminho = _gravar(tmp_path, "não é json")
    assert main([str(caminho), "--sem-espera"]) == 1
    saida = capsys.readouterr()
    assert saida.out == ""
    assert "linha 1" in saida.err


def test_main_reproducao_obedece_ao_interromper(monkeypatch):
    """A API manda o comando depois do primeiro passo, como no roteiro."""
    recebidas: list[str] = []
    app = FastAPI()

    @app.post("/telemetria")
    def telemetria(entrada: EntradaPonte) -> RespostaPonte:
        recebidas.append(entrada.linha)
        mensagens = [json.loads(linha) for linha in recebidas]
        passou = any(m["tipo"] == "passo" for m in mensagens)
        confirmada = any(m.get("origem") == "web" for m in mensagens)
        return RespostaPonte(comandos=[Comando()] if passou and not confirmada else [])

    monkeypatch.setattr(cli, "criar_cliente", lambda url: TestClient(app))
    argumentos = [str(FIRM02), "--saida", "http", "--url", "http://api", "--sem-espera"]
    assert main(argumentos) == 0
    mensagens = [ler_linha(linha) for linha in recebidas]
    falhas = [m for m in mensagens if m.tipo == "falha"]
    assert [f.origem for f in falhas] == ["web"]  # a falha automática gravada não sai
    assert [m.estado for m in mensagens if m.tipo == "tel"][-3:] == ["failed"] * 3
