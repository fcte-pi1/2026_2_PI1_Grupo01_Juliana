import json
import os
import select
import sys
import threading
import time
from datetime import datetime

import httpx
import pytest
import serial
from ponte.__main__ import Ponte
from ponte.config import ConfigPonte
from ponte.leitora import abrir_serial

pytestmark = pytest.mark.skipif(
    not sys.platform.startswith(("linux", "darwin")), reason="pty só existe em Linux e macOS"
)

INTERROMPER = b'{"v":1,"cmd":"interromper"}\n'
LINHAS = [json.dumps({"v": 1, "tipo": "tel", "t": indice}) for indice in range(5)]


@pytest.fixture
def pty():
    mestre, escravo = os.openpty()
    yield mestre, os.ttyname(escravo)
    os.close(mestre)
    os.close(escravo)


def montar(tmp_path, caminho, abrir):
    """Monta a ponte com um MockTransport que devolve interromper na terceira linha."""
    pedidos = []
    chegou = threading.Event()

    def responder(pedido):
        pedidos.append(json.loads(pedido.content))
        if len(pedidos) == len(LINHAS):
            chegou.set()
        comandos = [{"v": 1, "cmd": "interromper"}] if len(pedidos) == 3 else []
        return httpx.Response(200, json={"comandos": comandos})

    cliente = httpx.Client(
        base_url="http://api", transport=httpx.MockTransport(responder), timeout=2.0
    )
    config = ConfigPonte(_env_file=None, porta_serial=caminho, log_dir=tmp_path)
    return Ponte(config, cliente=cliente, abrir=abrir), pedidos, chegou


def abrir_avisando(aberta, falhas=0):
    """Abre a porta de verdade e sinaliza `aberta`; antes falha `falhas` vezes."""
    restantes = [falhas]

    def abrir(caminho):
        if restantes[0] > 0:
            restantes[0] -= 1
            raise serial.SerialException("queda simulada")
        porta = abrir_serial(caminho)
        aberta.set()
        return porta

    return abrir


def ler_do_mestre(mestre, esperado, limite):
    """Lê o lado mestre até receber `esperado` ou estourar `limite` segundos."""
    lido = b""
    fim = time.monotonic() + limite
    while esperado not in lido:
        restante = fim - time.monotonic()
        if restante <= 0:
            break
        prontos, _, _ = select.select([mestre], [], [], restante)
        if prontos:
            lido += os.read(mestre, 1024)
    return lido


def escrever_como_robo(mestre):
    """Escreve uma linha parcial e as 5 linhas; devolve o instante da terceira."""
    os.write(mestre, b'tipo":"tel"}\n')
    terceira = 0.0
    for indice, linha in enumerate(LINHAS):
        time.sleep(0.02)
        os.write(mestre, linha.encode() + b"\n")
        if indice == 2:
            terceira = time.monotonic()
    return terceira


def esperar_fila_vazia(ponte, limite=2.0):
    """O mock vê o pedido antes de a enviadora remover o item; espera a remoção."""
    fim = time.monotonic() + limite
    while len(ponte.fila) > 0:
        assert time.monotonic() < fim, "a fila não esvaziou"
        time.sleep(0.01)


def registros(ponte):
    return [linha.split(" ", 1)[1] for linha in ponte.auditoria.caminho.read_text().splitlines()]


def test_linhas_em_ordem_e_interromper_no_mestre(tmp_path, pty):
    mestre, caminho = pty
    aberta = threading.Event()
    ponte, pedidos, chegou = montar(tmp_path, caminho, abrir_avisando(aberta))
    ponte.iniciar()
    try:
        assert aberta.wait(2)
        terceira = escrever_como_robo(mestre)
        lido = ler_do_mestre(mestre, INTERROMPER, limite=1.0)
        assert lido == INTERROMPER
        assert time.monotonic() - terceira < 1.0
        assert chegou.wait(2)
        esperar_fila_vazia(ponte)
    finally:
        ponte.encerrar()

    assert [pedido["linha"] for pedido in pedidos] == LINHAS
    instantes = [datetime.fromisoformat(pedido["recebido_em"]) for pedido in pedidos]
    assert all(instante.tzinfo is not None for instante in instantes)
    assert instantes == sorted(instantes) and len(set(instantes)) == len(instantes)
    log = registros(ponte)
    assert 'TX {"v":1,"cmd":"interromper"}' in log
    assert log[-1] == "CONEXAO encerrada, 0 linhas na fila descartadas"


def test_volta_a_ler_depois_da_queda_sem_reiniciar(tmp_path, pty):
    mestre, caminho = pty
    aberta = threading.Event()
    ponte, pedidos, chegou = montar(tmp_path, caminho, abrir_avisando(aberta, falhas=1))
    ponte.iniciar()
    try:
        assert aberta.wait(3)
        escrever_como_robo(mestre)
        assert chegou.wait(2)
        esperar_fila_vazia(ponte)
    finally:
        ponte.encerrar()

    assert [pedido["linha"] for pedido in pedidos] == LINHAS
    log = registros(ponte)
    assert log[0] == "CONEXAO caiu queda simulada"
    assert log[1] == f"CONEXAO aberta {caminho}"
    for thread in ponte.threads:
        assert not thread.is_alive()
