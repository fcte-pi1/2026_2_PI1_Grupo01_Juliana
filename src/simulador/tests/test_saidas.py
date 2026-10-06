"""Testes da saída pela porta serial virtual (pty)."""

import json
import os
import sys
import threading
import time

import pytest

import simulador.__main__ as cli
from simulador.__main__ import main
from simulador.saidas import SaidaPty

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="pty só existe em POSIX")

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


class Ponte:
    """Faz o papel da ponte: abre o caminho do pty e lê tudo até o simulador fechar."""

    def __init__(self, caminho: str, enviar: tuple[bytes, ...] = (), atraso_s: float = 0):
        self.caminho = caminho
        self.enviar = enviar
        self.atraso_s = atraso_s
        self.recebido = b""
        self._thread = threading.Thread(target=self._rodar, daemon=True)
        self._thread.start()

    def _rodar(self) -> None:
        fd = os.open(self.caminho, os.O_RDWR | os.O_NOCTTY)
        try:
            for pedaco in self.enviar:
                os.write(fd, pedaco)
                time.sleep(0.05)
            time.sleep(self.atraso_s)
            while True:
                try:
                    dados = os.read(fd, 4096)
                except OSError:  # EIO: o simulador fechou o lado mestre
                    break
                if not dados:
                    break
                self.recebido += dados
        finally:
            os.close(fd)

    def linhas(self) -> list[str]:
        self._thread.join(timeout=5)
        assert not self._thread.is_alive()
        return self.recebido.decode().splitlines(keepends=True)


def test_nao_comeca_antes_de_alguem_abrir_a_porta():
    with SaidaPty() as saida:
        assert saida.caminho.startswith("/dev/")
        assert saida.aguardar_conexao(timeout_s=0.3) is False
        ponte = Ponte(saida.caminho)
        assert saida.aguardar_conexao(timeout_s=5) is True
        saida.escrever('{"v":1}\n')
    assert ponte.linhas() == ['{"v":1}\n']


def test_fechar_espera_a_ponte_ler_tudo():
    linhas = [f'{{"v":1,"seq":{i}}}\n' for i in range(100)]
    with SaidaPty() as saida:
        ponte = Ponte(saida.caminho, atraso_s=0.3)  # abre a porta, mas demora para ler
        assert saida.aguardar_conexao(timeout_s=5)
        for linha in linhas:
            saida.escrever(linha)
    assert ponte.linhas() == linhas


def test_linhas_do_outro_lado_chegam_ao_callback():
    recebidas: list[str] = []
    chegaram = threading.Event()

    def ao_receber(linha: str) -> None:
        recebidas.append(linha)
        if len(recebidas) == 2:
            chegaram.set()

    with SaidaPty(ao_receber) as saida:
        # A segunda linha chega em dois pedaços e precisa ser remontada.
        Ponte(saida.caminho, enviar=(b'{"v":1,"cmd":"interromper"}\r\n{"v":1,', b'"cmd":"x"}\n'))
        assert saida.aguardar_conexao(timeout_s=5)
        assert chegaram.wait(5)
    assert recebidas == ['{"v":1,"cmd":"interromper"}', '{"v":1,"cmd":"x"}']


def test_main_pty_entrega_as_mesmas_linhas_do_stdout(tmp_path, capsys, monkeypatch):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")
    assert main([str(caminho), "--saida", "stdout", "--sem-espera"]) == 0
    esperadas = capsys.readouterr().out.splitlines(keepends=True)

    pontes: list[Ponte] = []

    class SaidaComPonte(SaidaPty):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            pontes.append(Ponte(self.caminho))

    monkeypatch.setattr(cli, "SaidaPty", SaidaComPonte)
    assert main([str(caminho), "--saida", "pty", "--sem-espera"]) == 0
    assert pontes[0].linhas() == esperadas
    assert pontes[0].caminho in capsys.readouterr().err


def test_pty_no_windows_sai_com_1(tmp_path, capsys, monkeypatch):
    caminho = tmp_path / "roteiro.json"
    caminho.write_text(json.dumps(ROTEIRO), encoding="utf-8")
    monkeypatch.setattr(sys, "platform", "win32")

    assert main([str(caminho), "--saida", "pty", "--sem-espera"]) == 1
    assert "Windows" in capsys.readouterr().err
