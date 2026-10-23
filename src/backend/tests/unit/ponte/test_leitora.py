import threading
from datetime import datetime, timedelta, timezone

import serial
from ponte.auditoria import Auditoria
from ponte.espera import Espera
from ponte.fila import Fila
from ponte.leitora import Leitora

FUSO = timezone(timedelta(hours=-3))
INICIO = datetime(2026, 10, 21, 9, 0, 0, tzinfo=FUSO)


class Relogio:
    """Avança um segundo a cada leitura."""

    def __init__(self):
        self.segundos = 0

    def __call__(self):
        self.segundos += 1
        return INICIO + timedelta(seconds=self.segundos)


class PortaFalsa:
    """Devolve os pedaços em ordem; uma exceção na lista é levantada no lugar do pedaço."""

    def __init__(self, pedacos, parar=None):
        self.pedacos = list(pedacos)
        self.parar = parar
        self.escrito = b""
        self.fechada = False

    def readline(self):
        if not self.pedacos:
            if self.parar is not None:
                self.parar.set()
            return b""
        pedaco = self.pedacos.pop(0)
        if isinstance(pedaco, BaseException):
            raise pedaco
        return pedaco

    def write(self, dados):
        self.escrito += dados
        return len(dados)

    def close(self):
        self.fechada = True


def montar(tmp_path, aberturas, fila=None):
    """Monta a leitora com `aberturas`: portas falsas ou exceções, uma por tentativa."""
    aberturas = list(aberturas)
    esperas = []
    fila = Fila(100) if fila is None else fila
    auditoria = Auditoria(tmp_path, relogio=lambda: INICIO)

    def abrir(_porta):
        abertura = aberturas.pop(0)
        if isinstance(abertura, BaseException):
            raise abertura
        return abertura

    leitora = Leitora(
        "/dev/falsa",
        fila,
        auditoria,
        abrir=abrir,
        relogio=Relogio(),
        dormir=esperas.append,
    )
    return leitora, fila, auditoria, esperas


def linhas_da_fila(fila):
    itens = []
    while (item := fila.primeiro(timeout=0)) is not None:
        itens.append(item)
        fila.remover_primeiro()
    return itens


def log(auditoria):
    auditoria.fechar()
    return [linha.split(" ", 1)[1] for linha in auditoria.caminho.read_text().splitlines()]


def test_espera_crescente_e_zerar():
    espera = Espera()
    assert [espera.proxima() for _ in range(5)] == [0.5, 1, 2, 2, 2]
    espera.zerar()
    assert espera.proxima() == 0.5


def test_descarta_a_primeira_linha_e_junta_pedacos(tmp_path):
    parar = threading.Event()
    porta = PortaFalsa([b'o":"tel"}\n', b'{"v":1,', b'"tipo":"tel"}', b"\n"], parar)
    leitora, fila, auditoria, _ = montar(tmp_path, [porta])

    leitora.rodar(parar)

    assert [linha for linha, _ in linhas_da_fila(fila)] == ['{"v":1,"tipo":"tel"}']
    assert log(auditoria) == ["CONEXAO aberta /dev/falsa", 'RX {"v":1,"tipo":"tel"}']
    assert porta.fechada


def test_tira_crlf_ignora_vazia_e_troca_bytes_invalidos(tmp_path):
    parar = threading.Event()
    porta = PortaFalsa([b"lixo\n", b"a\r\n", b"\r\n", b"\n", b"b\xffc\n"], parar)
    leitora, fila, _, _ = montar(tmp_path, [porta])

    leitora.rodar(parar)

    assert [linha for linha, _ in linhas_da_fila(fila)] == ["a", "b�c"]


def test_recebido_em_quando_o_fim_da_linha_chega(tmp_path):
    parar = threading.Event()
    porta = PortaFalsa([b"lixo\n", b"a\n", b"b\n"], parar)
    leitora, fila, _, _ = montar(tmp_path, [porta])

    leitora.rodar(parar)

    itens = linhas_da_fila(fila)
    assert [recebido_em for _, recebido_em in itens] == [
        INICIO + timedelta(seconds=2),
        INICIO + timedelta(seconds=3),
    ]
    assert all(recebido_em.tzinfo is not None for _, recebido_em in itens)


def test_queda_e_reabertura_com_espera_crescente(tmp_path):
    parar = threading.Event()
    primeira = PortaFalsa([b"lixo\n", b"a\n", b"pela met", serial.SerialException("sumiu")])
    segunda = PortaFalsa([b"ade\n", b"b\n"], parar)
    falha = OSError("sem porta")
    leitora, fila, auditoria, esperas = montar(
        tmp_path, [falha, falha, falha, falha, primeira, segunda]
    )

    leitora.rodar(parar)

    assert esperas == [0.5, 1, 2, 2, 0.5]
    assert [linha for linha, _ in linhas_da_fila(fila)] == ["a", "b"]
    assert primeira.fechada and segunda.fechada
    assert log(auditoria) == [
        "CONEXAO caiu sem porta",
        "CONEXAO caiu sem porta",
        "CONEXAO caiu sem porta",
        "CONEXAO caiu sem porta",
        "CONEXAO aberta /dev/falsa",
        "RX a",
        "CONEXAO caiu sumiu",
        "CONEXAO aberta /dev/falsa",
        "RX b",
    ]


def test_descarte_da_fila_vai_para_o_log(tmp_path):
    parar = threading.Event()
    porta = PortaFalsa([b"lixo\n", b'{"tipo":"inicio"}\n', b'{"tipo":"tel"}\n'], parar)
    leitora, _, auditoria, _ = montar(tmp_path, [porta], fila=Fila(1))

    leitora.rodar(parar)

    assert log(auditoria)[-1] == 'FILA descartada evento {"tipo":"inicio"}'


def test_escrever_na_porta_aberta(tmp_path):
    parar = threading.Event()
    escrito = []

    class PortaQueEscreve(PortaFalsa):
        def readline(self):
            escrito.append(leitora.escrever('{"v":1,"cmd":"interromper"}\n'))
            escrito.append(self.escrito)
            parar.set()
            return b""

    porta = PortaQueEscreve([])
    leitora, _, _, _ = montar(tmp_path, [porta])

    leitora.rodar(parar)

    assert escrito == [True, b'{"v":1,"cmd":"interromper"}\n']


def test_escrever_sem_porta_registra_comando_perdido(tmp_path):
    leitora, _, auditoria, _ = montar(tmp_path, [])

    assert leitora.escrever('{"v":1,"cmd":"interromper"}\n') is False

    assert log(auditoria) == ['CONEXAO sem porta, comando perdido {"v":1,"cmd":"interromper"}']


def test_erro_inesperado_na_leitura_reabre_a_porta(tmp_path):
    parar = threading.Event()
    primeira = PortaFalsa([b"lixo\n", RuntimeError("inesperado")])
    segunda = PortaFalsa([b"lixo\n", b"b\n"], parar)
    leitora, fila, auditoria, esperas = montar(
        tmp_path, [ValueError("porta estranha"), primeira, segunda]
    )

    leitora.rodar(parar)

    assert esperas == [0.5, 0.5]
    assert [linha for linha, _ in linhas_da_fila(fila)] == ["b"]
    assert log(auditoria) == [
        "CONEXAO caiu porta estranha",
        "CONEXAO aberta /dev/falsa",
        "CONEXAO caiu inesperado",
        "CONEXAO aberta /dev/falsa",
        "RX b",
    ]
