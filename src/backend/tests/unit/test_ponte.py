"""Ponte: encaminha a linha crua e devolve os comandos, inclusive no 422."""

import json
from collections import deque
from datetime import UTC, datetime
from io import StringIO

from ponte.encaminhar import FILA_MAXIMA, encaminhar, executar, normalizar

MOMENTO = datetime(2026, 10, 24, 14, 0, tzinfo=UTC)
LINHA = (
    '{"v":1,"boot":1,"seq":0,"t_ms":0,"tipo":"hc_resultado",'
    '"aprovado":true,"tipo_dip":"4x4","inicio":"retomada"}\n'
)


class Resposta:
    def __init__(self, status: int, corpo: dict) -> None:
        self.status_code = status
        self._corpo = corpo

    def json(self) -> dict:
        return self._corpo


class Cliente:
    def __init__(self, status: int = 200, comandos: list | None = None) -> None:
        self.status = status
        self.comandos = comandos or []
        self.enviadas: list[str] = []

    def post(self, caminho: str, *, json: dict) -> Resposta:
        assert caminho == "/telemetria"
        self.enviadas.append(json["linha"])
        assert json["recebido_em"] == MOMENTO.isoformat()
        return Resposta(self.status, {"comandos": self.comandos})


def test_ignora_vazio_e_depuracao():
    assert normalizar("\n") is None
    assert normalizar("  # tof_mm f=1\n") is None
    assert normalizar('{"v":1}') == '{"v":1}\n'


def test_encaminha_e_escreve_o_comando():
    cliente = Cliente(comandos=[{"v": 1, "cmd": "largar"}])
    destino = StringIO()

    enviadas = executar([LINHA, "# debug\n", "\n"], destino, cliente, agora=MOMENTO)

    assert enviadas == 1
    assert cliente.enviadas == [LINHA]
    assert json.loads(destino.getvalue()) == {"v": 1, "cmd": "largar"}


def test_422_tambem_devolve_os_comandos():
    cliente = Cliente(status=422, comandos=[{"v": 1, "cmd": "health_check"}])

    status, comandos = encaminhar(cliente, LINHA, MOMENTO)

    assert status == 422
    assert comandos == [{"v": 1, "cmd": "health_check"}]


class _Instavel:
    """Falha as primeiras tentativas e depois aceita, guardando a ordem."""

    def __init__(self, falhas: int, *, erro: BaseException | None = None) -> None:
        self.falhas = falhas
        self.erro = erro
        self.enviadas: list[str] = []

    def post(self, caminho: str, *, json: dict) -> Resposta:
        assert caminho == "/telemetria"
        assert json["recebido_em"] == MOMENTO.isoformat()
        if self.falhas:
            self.falhas -= 1
            if self.erro is not None:
                raise self.erro
            return Resposta(503, {"comandos": [{"v": 1, "cmd": "largar"}]})
        self.enviadas.append(json["linha"])
        return Resposta(200, {"comandos": []})


def test_5xx_guarda_a_linha_e_reenvia_em_ordem():
    cliente = _Instavel(falhas=2)
    destino = StringIO()

    enviadas = executar(
        ['{"seq":0}\n', '{"seq":1}\n', '{"seq":2}\n'],
        destino,
        cliente,
        agora=MOMENTO,
    )

    assert enviadas == 3
    assert cliente.enviadas == ['{"seq":0}\n', '{"seq":1}\n', '{"seq":2}\n']
    assert destino.getvalue() == ""


def test_api_fora_do_ar_guarda_e_reenvia():
    cliente = _Instavel(falhas=1, erro=ConnectionError("recusada"))
    destino = StringIO()

    enviadas = executar(['{"seq":0}\n', '{"seq":1}\n'], destino, cliente, agora=MOMENTO)

    assert enviadas == 2
    assert cliente.enviadas == ['{"seq":0}\n', '{"seq":1}\n']


def test_422_nao_volta_para_a_fila():
    class RecusaUmaVez:
        def __init__(self) -> None:
            self.enviadas: list[str] = []

        def post(self, caminho: str, *, json: dict) -> Resposta:
            self.enviadas.append(json["linha"])
            status = 422 if len(self.enviadas) == 1 else 200
            return Resposta(status, {"comandos": []})

    cliente = RecusaUmaVez()
    executar(['{"seq":0}\n', '{"seq":1}\n'], StringIO(), cliente, agora=MOMENTO)

    assert cliente.enviadas == ['{"seq":0}\n', '{"seq":1}\n']


def test_fila_cheia_descarta_a_mais_antiga():
    cliente = _Instavel(falhas=99)
    pendentes: deque[tuple[str, datetime]] = deque()

    enviadas = executar(
        [f'{{"seq":{seq}}}\n' for seq in range(5)],
        StringIO(),
        cliente,
        agora=MOMENTO,
        fila_maxima=3,
        pendentes=pendentes,
    )

    assert enviadas == 0
    assert [linha for linha, _ in pendentes] == ['{"seq":2}\n', '{"seq":3}\n', '{"seq":4}\n']


def test_fila_maxima_padrao_e_100():
    assert FILA_MAXIMA == 100
