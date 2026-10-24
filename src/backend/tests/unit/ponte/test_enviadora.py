import json
import threading
from datetime import datetime, timedelta, timezone

import httpx
from ponte.auditoria import Auditoria
from ponte.enviadora import Enviadora
from ponte.fila import Fila

FUSO = timezone(timedelta(hours=-3))
INICIO = datetime(2026, 10, 21, 9, 0, 0, tzinfo=FUSO)
INTERROMPER = {"v": 1, "cmd": "interromper"}


def montar(tmp_path, respostas, linhas=("a",)):
    """Monta a enviadora com um MockTransport que devolve `respostas` em ordem.

    Cada resposta é um par (status, corpo) ou uma exceção levantada no lugar do envio.
    """
    respostas = list(respostas)
    pedidos = []

    def responder(pedido):
        pedidos.append(json.loads(pedido.content))
        resposta = respostas.pop(0)
        if isinstance(resposta, BaseException):
            raise resposta
        status, corpo = resposta
        return httpx.Response(status, json=corpo)

    cliente = httpx.Client(
        base_url="http://api", transport=httpx.MockTransport(responder), timeout=2.0
    )
    fila = Fila(100)
    for indice, linha in enumerate(linhas):
        fila.colocar((linha, INICIO + timedelta(seconds=indice)))
    auditoria = Auditoria(tmp_path, relogio=lambda: INICIO)
    escritas = []
    esperas = []

    def escrever(linha):
        escritas.append(linha)
        return True

    enviadora = Enviadora(cliente, fila, auditoria, escrever, dormir=esperas.append)
    return enviadora, fila, auditoria, pedidos, escritas, esperas


def esvaziar(enviadora, fila, limite=20):
    for _ in range(limite):
        if len(fila) == 0:
            return
        enviadora.enviar_primeiro(timeout=0)
    raise AssertionError("a fila não esvaziou")


def registros(auditoria):
    return [linha.split(" ", 1)[1] for linha in auditoria.caminho.read_text().splitlines()]


def test_200_sem_comandos(tmp_path):
    enviadora, fila, auditoria, pedidos, escritas, _ = montar(tmp_path, [(200, {"comandos": []})])
    esvaziar(enviadora, fila)
    assert pedidos == [{"linha": "a", "recebido_em": "2026-10-21T09:00:00-03:00"}]
    assert escritas == []
    assert registros(auditoria) == ["API 200 comandos=0"]


def test_200_com_interromper(tmp_path):
    enviadora, fila, auditoria, _, escritas, _ = montar(
        tmp_path, [(200, {"comandos": [INTERROMPER]})]
    )
    esvaziar(enviadora, fila)
    assert escritas == ['{"v":1,"cmd":"interromper"}\n']
    assert registros(auditoria) == ["API 200 comandos=1", 'TX {"v":1,"cmd":"interromper"}']


def test_422_com_comandos_escreve_os_comandos(tmp_path):
    corpo = {"detail": "linha com 300 bytes (máximo 256)", "comandos": [INTERROMPER]}
    enviadora, fila, auditoria, _, escritas, esperas = montar(tmp_path, [(422, corpo)])
    esvaziar(enviadora, fila)
    assert escritas == ['{"v":1,"cmd":"interromper"}\n']
    assert esperas == []
    assert registros(auditoria)[0] == "API 422 comandos=1"


def test_422_do_fastapi_sem_comandos_remove_o_item(tmp_path):
    corpo = {"detail": [{"type": "missing", "loc": ["body", "linha"], "msg": "Field required"}]}
    enviadora, fila, auditoria, pedidos, escritas, esperas = montar(tmp_path, [(422, corpo)])
    esvaziar(enviadora, fila)
    assert len(pedidos) == 1
    assert escritas == []
    assert esperas == []
    assert registros(auditoria) == ["API 422 comandos=0"]


def test_400_nao_le_comandos_e_nao_reenvia(tmp_path):
    enviadora, fila, auditoria, pedidos, escritas, _ = montar(
        tmp_path, [(400, {"comandos": [INTERROMPER]})]
    )
    esvaziar(enviadora, fila)
    assert len(pedidos) == 1
    assert escritas == []
    assert registros(auditoria) == ["API 400 comandos=0"]


def test_500_seguido_de_200_reenvia_a_mesma_linha_uma_vez(tmp_path):
    enviadora, fila, auditoria, pedidos, _, esperas = montar(
        tmp_path,
        [(500, {}), (200, {"comandos": []}), (200, {"comandos": []})],
        linhas=("a", "b"),
    )
    esvaziar(enviadora, fila)
    assert [pedido["linha"] for pedido in pedidos] == ["a", "a", "b"]
    assert esperas == [0.5]
    assert registros(auditoria) == [
        "API falhou status 500",
        "API 200 comandos=0",
        "API 200 comandos=0",
    ]


def test_timeout_mantem_o_item_e_espera_cresce_e_zera(tmp_path):
    enviadora, fila, auditoria, pedidos, _, esperas = montar(
        tmp_path,
        [
            httpx.ReadTimeout("estourou"),
            httpx.ConnectError("recusada"),
            (503, {}),
            (200, {}),
            httpx.ReadTimeout("estourou"),
            (200, {}),
        ],
        linhas=("a", "b"),
    )
    esvaziar(enviadora, fila)
    assert [pedido["linha"] for pedido in pedidos] == ["a", "a", "a", "a", "b", "b"]
    assert esperas == [0.5, 1.0, 2.0, 0.5]
    assert registros(auditoria)[:3] == [
        "API falhou ReadTimeout estourou",
        "API falhou ConnectError recusada",
        "API falhou status 503",
    ]


def test_comando_com_cmd_desconhecido_e_recusado(tmp_path):
    enviadora, fila, auditoria, _, escritas, _ = montar(
        tmp_path, [(200, {"comandos": [{"v": 1, "cmd": "acelerar"}, INTERROMPER]})]
    )
    esvaziar(enviadora, fila)
    assert escritas == ['{"v":1,"cmd":"interromper"}\n']
    assert registros(auditoria) == [
        "API 200 comandos=2",
        'API comando recusado {"v":1,"cmd":"acelerar"}',
        'TX {"v":1,"cmd":"interromper"}',
    ]


def test_comando_sem_v_ou_com_v_texto_e_recusado(tmp_path):
    enviadora, fila, auditoria, _, escritas, _ = montar(
        tmp_path,
        [(200, {"comandos": [{"cmd": "interromper"}, {"v": "1", "cmd": "largar"}, "largar"]})],
    )
    esvaziar(enviadora, fila)
    assert escritas == []
    assert registros(auditoria) == [
        "API 200 comandos=3",
        'API comando recusado {"cmd":"interromper"}',
        'API comando recusado {"v":"1","cmd":"largar"}',
        'API comando recusado "largar"',
    ]


def test_ordem_preservada(tmp_path):
    linhas = ("a", "b", "c", "d")
    enviadora, fila, _, pedidos, _, _ = montar(
        tmp_path, [(200, {"comandos": []})] * len(linhas), linhas=linhas
    )
    esvaziar(enviadora, fila)
    assert [pedido["linha"] for pedido in pedidos] == list(linhas)
    assert [pedido["recebido_em"][17:19] for pedido in pedidos] == ["00", "01", "02", "03"]


def test_item_descartado_durante_o_reenvio_nao_remove_outro(tmp_path):
    fila = Fila(1)
    fila.colocar(("a", INICIO))
    enviadas = []

    def responder_e_descartar(pedido):
        # Enquanto "a" está sendo enviado, a leitora põe "b" e a fila cheia descarta "a".
        fila.colocar(("b", INICIO))
        enviadas.append(json.loads(pedido.content)["linha"])
        return httpx.Response(200, json={"comandos": []})

    cliente = httpx.Client(
        base_url="http://api", transport=httpx.MockTransport(responder_e_descartar)
    )
    auditoria = Auditoria(tmp_path, relogio=lambda: INICIO)
    enviadora = Enviadora(cliente, fila, auditoria, lambda _linha: None, dormir=lambda _s: None)
    enviadora.enviar_primeiro(timeout=0)
    assert enviadas == ["a"]
    assert fila.primeiro(timeout=0) == ("b", INICIO)


def test_tx_so_e_registrado_quando_a_escrita_acontece(tmp_path):
    enviadora, fila, auditoria, _, _, _ = montar(tmp_path, [(200, {"comandos": [INTERROMPER]})])
    enviadora._escrever = lambda _linha: False
    esvaziar(enviadora, fila)
    assert registros(auditoria) == ["API 200 comandos=1"]


def test_erro_inesperado_e_registrado_e_a_enviadora_continua(tmp_path):
    parar = threading.Event()
    enviadora, fila, auditoria, pedidos, _, esperas = montar(
        tmp_path, [httpx.DecodingError("corpo corrompido"), (200, {"comandos": []})]
    )
    remover = fila.remover_primeiro

    def remover_e_parar():
        remover()
        parar.set()

    fila.remover_primeiro = remover_e_parar
    enviadora.rodar(parar)

    assert len(pedidos) == 2
    assert len(fila) == 0
    assert esperas == [0.5]
    assert registros(auditoria) == [
        "API erro inesperado DecodingError corpo corrompido",
        "API 200 comandos=0",
    ]
