"""Ponte: encaminha a linha crua e devolve os comandos, inclusive no 422."""

import json
from datetime import UTC, datetime
from io import StringIO

from ponte.encaminhar import encaminhar, executar, normalizar

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
