"""Testes da reação ao comando interromper: emissor, pty e HTTP."""

import json
import logging
import os
import sys
import threading
from pathlib import Path

import pytest
from contrato.telemetria import Comando, EntradaPonte, Falha, RespostaPonte, Tel, ler_linha
from fastapi import FastAPI
from fastapi.testclient import TestClient

import simulador.__main__ as cli
from simulador.__main__ import main
from simulador.emissor import emitir
from simulador.gerador import gerar
from simulador.interrupcao import Interrupcao
from simulador.roteiro import carregar
from simulador.saidas import SaidaPty

SUCESSO_4X4 = Path(__file__).parents[1] / "roteiros" / "sucesso-4x4.json"
INTERROMPER = '{"v":1,"cmd":"interromper"}'


class RelogioFalso:
    """Relógio que só anda quando alguém dorme."""

    def __init__(self):
        self.t = 0.0

    def agora(self) -> float:
        return self.t

    def dormir(self, segundos: float) -> None:
        self.t += segundos


def _rodar(gatilho, comandos=(INTERROMPER,), depois=None):
    """Emite o sucesso-4x4 e manda `comandos` logo depois da linha em que `gatilho` for True.

    `depois(mensagem)` é chamado para cada linha que sai após a primeira falha web e pode
    devolver mais comandos.
    """
    interrupcao = Interrupcao()
    relogio = RelogioFalso()
    saida: list[tuple[float, object]] = []
    parado = False

    def escrever(linha: str) -> None:
        nonlocal parado
        mensagem = ler_linha(linha)
        saida.append((relogio.agora(), mensagem))
        novos: tuple[str, ...] = ()
        if parado and depois is not None:
            novos = depois(mensagem)
        elif not parado and gatilho(mensagem):
            novos = comandos
        parado = parado or (isinstance(mensagem, Falha) and mensagem.origem == "web")
        for comando in novos:
            interrupcao.ao_receber(comando)

    emitir(gerar(carregar(SUCESSO_4X4)), escrever, relogio, interrupcao=interrupcao)
    return saida


def _segundo_passo(mensagem) -> bool:
    return mensagem.tipo == "passo" and (mensagem.x, mensagem.y) == (0, 1)


def test_interromper_para_e_confirma_na_celula_atual():
    saida = _rodar(_segundo_passo)
    mensagens = [mensagem for _, mensagem in saida]
    i = next(i for i, m in enumerate(mensagens) if m.tipo == "falha")
    falha = mensagens[i]

    assert falha.origem == "web"
    assert falha.motivo is None and falha.componente is None
    assert (falha.x, falha.y) == (0, 1)
    assert falha.seq == mensagens[i - 1].seq + 1
    assert not any(m.tipo in ("passo", "sucesso") for m in mensagens[i:])
    # a tel seguinte, a 200 ms, não saiu: a falha vai no lugar dela
    assert falha.t_ms == mensagens[i - 1].t_ms + 200

    depois = mensagens[i + 1 :]
    assert [m.tipo for m in depois] == ["tel"] * 3
    assert all(m.estado == "failed" and m.vel_mm_s == 0 for m in depois)
    assert all((m.x, m.y) == (0, 1) for m in depois)
    assert [m.t_ms - falha.t_ms for m in depois] == [1000, 2000, 3000]
    assert [m.seq for m in depois] == [falha.seq + 1, falha.seq + 2, falha.seq + 3]
    instantes = [t for t, _ in saida[i:]]
    assert [round(t - instantes[0], 3) for t in instantes] == [0, 1, 2, 3]


def test_novo_interromper_depois_de_parado_so_repete_a_falha():
    def repetir_uma_vez(mensagem):
        return (INTERROMPER,) if mensagem.tipo == "tel" and mensagem.seq % 2 == 0 else ()

    saida = _rodar(_segundo_passo, depois=repetir_uma_vez)
    mensagens = [mensagem for _, mensagem in saida]
    falhas = [m for m in mensagens if m.tipo == "falha"]
    tels = [m for m in mensagens if m.tipo == "tel" and m.estado == "failed"]

    assert len(falhas) >= 2
    assert all(f == falhas[0] for f in falhas)  # a mesma mensagem, com o mesmo seq
    assert len(tels) == 3
    assert [t.seq for t in tels] == [falhas[0].seq + k for k in (1, 2, 3)]


def test_varios_comandos_juntos_param_uma_vez_so():
    saida = _rodar(_segundo_passo, comandos=(INTERROMPER, INTERROMPER))
    assert sum(m.tipo == "falha" for _, m in saida) == 1


@pytest.mark.parametrize(
    "linha",
    [
        '{"v":2,"cmd":"interromper"}',
        '{"v":1,"cmd":"pausar"}',
        '{"cmd":"interromper"}',
        '{"v":"1","cmd":"interromper"}',
        "interromper",
        "[]",
    ],
)
def test_comando_fora_do_contrato_e_ignorado_e_registrado(linha, monkeypatch, caplog):
    monkeypatch.setattr(logging.getLogger("simulador"), "propagate", True)
    caplog.set_level(logging.WARNING, logger="simulador")

    saida = _rodar(_segundo_passo, comandos=(linha,))

    assert not any(m.tipo == "falha" for _, m in saida)
    assert saida[-1][1].tipo == "tel" and saida[-1][1].estado == "success"
    assert linha in caplog.text


def test_interromper_depois_do_fim_nao_tem_efeito():
    saida = _rodar(lambda m: m.tipo == "sucesso")
    tipos = [m.tipo for _, m in saida]
    assert "falha" not in tipos
    assert tipos[-4:] == ["sucesso", "tel", "tel", "tel"]


def test_comando_valido_e_registrado(monkeypatch, caplog):
    monkeypatch.setattr(logging.getLogger("simulador"), "propagate", True)
    caplog.set_level(logging.INFO, logger="simulador")
    interrupcao = Interrupcao()
    interrupcao.ao_receber(INTERROMPER)
    assert interrupcao.novos() == 1
    assert interrupcao.novos() == 0
    assert INTERROMPER in caplog.text


def _conferir_interrupcao(linhas: list[str]) -> None:
    mensagens = [ler_linha(linha) for linha in linhas]
    falhas = [m for m in mensagens if isinstance(m, Falha)]
    assert falhas and all(f == falhas[0] and f.origem == "web" for f in falhas)
    assert not any(m.tipo == "sucesso" for m in mensagens)
    i = mensagens.index(falhas[0])
    anterior = mensagens[i - 1]
    assert (falhas[0].x, falhas[0].y) == (anterior.x, anterior.y)
    tels = [m for m in mensagens[i:] if isinstance(m, Tel)]
    assert [t.estado for t in tels] == ["failed"] * 3


@pytest.mark.skipif(sys.platform == "win32", reason="pty só existe em POSIX")
def test_main_pty_interrompe(monkeypatch):
    pontes: list[PonteQueInterrompe] = []

    class SaidaComPonte(SaidaPty):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            pontes.append(PonteQueInterrompe(self.caminho))

    monkeypatch.setattr(cli, "SaidaPty", SaidaComPonte)
    assert main([str(SUCESSO_4X4), "--saida", "pty", "--acelerar", "10"]) == 0
    _conferir_interrupcao(pontes[0].linhas())


class PonteQueInterrompe:
    """Ponte que lê o pty e manda o interromper ao ver o primeiro passo."""

    def __init__(self, caminho: str):
        self.caminho = caminho
        self.recebido = b""
        self._thread = threading.Thread(target=self._rodar, daemon=True)
        self._thread.start()

    def _rodar(self) -> None:
        fd = os.open(self.caminho, os.O_RDWR | os.O_NOCTTY)
        enviado = False
        try:
            while True:
                try:
                    dados = os.read(fd, 4096)
                except OSError:  # EIO: o simulador fechou o lado mestre
                    break
                if not dados:
                    break
                self.recebido += dados
                if not enviado and b'"tipo":"passo"' in self.recebido:
                    os.write(fd, INTERROMPER.encode() + b"\n")
                    enviado = True
        finally:
            os.close(fd)

    def linhas(self) -> list[str]:
        self._thread.join(timeout=10)
        assert not self._thread.is_alive()
        return self.recebido.decode().splitlines()


def test_main_http_interrompe(monkeypatch):
    """A API devolve o comando em toda resposta até chegar a falha web, como o backend."""
    linhas: list[str] = []
    app = FastAPI()

    @app.post("/telemetria")
    def telemetria(entrada: EntradaPonte) -> RespostaPonte:
        linhas.append(entrada.linha)
        mensagens = [json.loads(linha) for linha in linhas]
        confirmada = any(m.get("origem") == "web" for m in mensagens)
        passou = any(m["tipo"] == "passo" for m in mensagens)
        return RespostaPonte(comandos=[Comando()] if passou and not confirmada else [])

    monkeypatch.setattr(cli, "criar_cliente", lambda url: TestClient(app))
    assert main([str(SUCESSO_4X4), "--saida", "http", "--url", "http://api", "--sem-espera"]) == 0
    _conferir_interrupcao(linhas)
    mensagens = [ler_linha(linha) for linha in linhas]
    falha = next(m for m in mensagens if isinstance(m, Falha))
    assert (falha.x, falha.y) == (0, 0)  # o comando chega na resposta do primeiro passo
