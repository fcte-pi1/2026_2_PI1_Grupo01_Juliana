"""Testes da saída HTTP (POST /telemetria) com um app FastAPI mínimo."""

import json
import logging
from datetime import timedelta

import httpx
import pytest
from contrato.telemetria import Comando, EntradaPonte, RespostaPonte
from fastapi import FastAPI, Response
from fastapi.testclient import TestClient

import simulador.__main__ as cli
from simulador.__main__ import main
from simulador.saidas import SaidaHttp

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


class Api:
    """App mínimo com a rota da #119: guarda as entradas e pode falhar ou mandar comandos."""

    def __init__(self):
        self.entradas: list[EntradaPonte] = []
        self.falhar = False
        self.comandos: list[Comando] = []
        self.app = FastAPI()

        @self.app.post("/telemetria")
        def telemetria(entrada: EntradaPonte, response: Response) -> RespostaPonte:
            if self.falhar:
                response.status_code = 503
                return RespostaPonte()
            self.entradas.append(entrada)
            comandos, self.comandos = self.comandos, []
            return RespostaPonte(comandos=comandos)

    def cliente(self) -> TestClient:
        return TestClient(self.app)

    def linhas(self) -> list[str]:
        return [entrada.linha for entrada in self.entradas]


@pytest.fixture
def log_simulador(monkeypatch, caplog):
    """Deixa o caplog ver o logger "simulador", que o main() tira da propagação."""
    monkeypatch.setattr(logging.getLogger("simulador"), "propagate", True)
    caplog.set_level(logging.WARNING, logger="simulador")
    return caplog


def test_cada_linha_vira_um_post_com_fuso():
    api = Api()
    with SaidaHttp(api.cliente()) as saida:
        saida.escrever('{"v":1,"seq":0}\n')
        saida.escrever('{"v":1,"seq":1}\n')
    assert api.linhas() == ['{"v":1,"seq":0}', '{"v":1,"seq":1}']
    for entrada in api.entradas:
        assert entrada.recebido_em.utcoffset() == timedelta(hours=-3)


def test_comandos_da_resposta_chegam_ao_callback():
    api = Api()
    recebidas: list[str] = []
    with SaidaHttp(api.cliente(), ao_receber=recebidas.append) as saida:
        api.comandos = [Comando()]
        saida.escrever('{"v":1,"seq":0}\n')
        saida.escrever('{"v":1,"seq":1}\n')
    assert [json.loads(linha) for linha in recebidas] == [{"v": 1, "cmd": "interromper"}]


def test_5xx_guarda_a_linha_e_reenvia_em_ordem(log_simulador):
    api = Api()
    with SaidaHttp(api.cliente()) as saida:
        api.falhar = True
        saida.escrever('{"seq":0}\n')
        saida.escrever('{"seq":1}\n')
        assert api.linhas() == []
        assert len(saida.fila) == 2
        api.falhar = False
        saida.escrever('{"seq":2}\n')
    assert api.linhas() == ['{"seq":0}', '{"seq":1}', '{"seq":2}']
    assert "503" in log_simulador.text


@pytest.mark.parametrize(
    "erro",
    [httpx.ConnectError("recusada"), httpx.ReadTimeout("demorou")],
    ids=["recusa", "timeout"],
)
def test_api_fora_do_ar_guarda_e_reenvia(erro, log_simulador):
    api = Api()
    real = api.cliente()
    fora = True

    def transporte(pedido: httpx.Request) -> httpx.Response:
        if fora:
            raise erro
        return real.post(pedido.url.path, content=pedido.content, headers=pedido.headers)

    cliente = httpx.Client(base_url="http://api", transport=httpx.MockTransport(transporte))
    with SaidaHttp(cliente) as saida:
        saida.escrever('{"seq":0}\n')
        assert len(saida.fila) == 1
        fora = False
        saida.escrever('{"seq":1}\n')
    assert api.linhas() == ['{"seq":0}', '{"seq":1}']
    assert "não respondeu" in log_simulador.text


def test_fila_cheia_descarta_a_mais_antiga(log_simulador):
    api = Api()
    api.falhar = True
    saida = SaidaHttp(api.cliente(), fila_maxima=3)
    for seq in range(5):
        saida.escrever(f'{{"seq":{seq}}}\n')
    assert [entrada.linha for entrada in saida.fila] == ['{"seq":2}', '{"seq":3}', '{"seq":4}']
    assert "descartada" in log_simulador.text
    api.falhar = False
    saida.fechar()
    assert api.linhas() == ['{"seq":2}', '{"seq":3}', '{"seq":4}']


def test_fila_maxima_padrao_e_100():
    assert SaidaHttp(Api().cliente()).fila_maxima == 100


def test_main_http_envia_as_mesmas_linhas_do_stdout(tmp_path, capsys, monkeypatch):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")
    assert main([str(caminho), "--saida", "stdout", "--sem-espera", "--boot", "0"]) == 0
    esperadas = capsys.readouterr().out.splitlines()

    api = Api()
    urls: list[str] = []

    def criar_cliente(url: str) -> TestClient:
        urls.append(url)
        return api.cliente()

    monkeypatch.setattr(cli, "criar_cliente", criar_cliente)
    monkeypatch.setenv("API_URL", "http://api:8000")
    assert main([str(caminho), "--saida", "http", "--sem-espera", "--boot", "0"]) == 0
    assert urls == ["http://api:8000"]
    assert api.linhas() == esperadas


def test_main_http_sem_url_sai_com_1(tmp_path, capsys):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")
    assert main([str(caminho), "--saida", "http", "--sem-espera"]) == 1
    assert "API_URL" in capsys.readouterr().err
