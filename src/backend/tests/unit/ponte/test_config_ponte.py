from pathlib import Path

import pytest
from ponte.__main__ import main, montar_config
from ponte.config import ConfigPonte

VARIAVEIS = ("API_URL", "PONTE_PORTA_SERIAL", "PONTE_FILA_MAXIMA", "PONTE_LOG_DIR")


@pytest.fixture(autouse=True)
def ambiente_limpo(monkeypatch):
    for nome in VARIAVEIS:
        monkeypatch.delenv(nome, raising=False)


def test_padroes():
    config = ConfigPonte(_env_file=None)
    assert config.api_url == "http://localhost:8000"
    assert config.porta_serial == "/dev/rfcomm0"
    assert config.fila_maxima == 100
    assert config.log_dir == Path("logs/ponte")


def test_variaveis_de_ambiente(monkeypatch):
    monkeypatch.setenv("API_URL", "http://api:9000")
    monkeypatch.setenv("PONTE_PORTA_SERIAL", "COM5")
    monkeypatch.setenv("PONTE_FILA_MAXIMA", "7")
    monkeypatch.setenv("PONTE_LOG_DIR", "/tmp/ponte")
    config = montar_config([], env_file=None)
    assert config.api_url == "http://api:9000"
    assert config.porta_serial == "COM5"
    assert config.fila_maxima == 7
    assert config.log_dir == Path("/tmp/ponte")


def test_opcao_vence_variavel(monkeypatch):
    monkeypatch.setenv("API_URL", "http://api:9000")
    monkeypatch.setenv("PONTE_PORTA_SERIAL", "COM5")
    monkeypatch.setenv("PONTE_FILA_MAXIMA", "7")
    monkeypatch.setenv("PONTE_LOG_DIR", "/tmp/ponte")
    argv = ["--url", "http://x:1", "--porta", "/dev/pts/3", "--fila", "2", "--log-dir", "saida"]
    config = montar_config(argv, env_file=None)
    assert config.api_url == "http://x:1"
    assert config.porta_serial == "/dev/pts/3"
    assert config.fila_maxima == 2
    assert config.log_dir == Path("saida")


def test_opcao_parcial_mantem_variavel(monkeypatch):
    monkeypatch.setenv("PONTE_PORTA_SERIAL", "COM5")
    config = montar_config(["--url", "http://x:1"], env_file=None)
    assert config.api_url == "http://x:1"
    assert config.porta_serial == "COM5"


@pytest.mark.parametrize("fila", ["0", "-3"])
def test_fila_menor_que_um_sai_com_erro(fila, capsys):
    assert main(["--fila", fila]) == 1
    assert "fila" in capsys.readouterr().err
