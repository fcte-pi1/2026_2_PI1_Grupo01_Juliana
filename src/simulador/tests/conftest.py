"""Configuração comum dos testes do simulador."""

import pytest

VARIAVEIS = ("API_URL", "SIMULADOR_SAIDA", "SIMULADOR_ACELERAR", "SIMULADOR_TAXA_TEL_HZ")


@pytest.fixture(autouse=True)
def ambiente_limpo(monkeypatch):
    """Tira do ambiente as variáveis do .env, para os testes não dependerem da máquina."""
    for nome in VARIAVEIS:
        monkeypatch.delenv(nome, raising=False)


@pytest.fixture(autouse=True)
def arquivo_boot(monkeypatch, tmp_path):
    """Boot persistido num diretório temporário, fora do .simulador/ da máquina."""
    arquivo = tmp_path / ".simulador" / "boot"
    monkeypatch.setattr("simulador.__main__.ARQUIVO_BOOT", arquivo)
    return arquivo
