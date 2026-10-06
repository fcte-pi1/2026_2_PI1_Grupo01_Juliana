"""Interrupção pela web: o comando `interromper` e a falha que confirma a parada.

`Interrupcao` recebe as linhas que chegam pela saída (pty ou resposta HTTP), possivelmente
de outra thread, e conta os comandos válidos. `Situacao` acompanha o que o robô já enviou,
para montar a `falha` com origem web e a `tel` parada a partir do último estado visto pela web.
"""

import json
import logging
import threading

from contrato.telemetria import VERSAO, Comando, Falha, HcResultado, Mensagem, Sucesso, Tel
from pydantic import ValidationError

from simulador.gerador import Item, LinhaCrua

log = logging.getLogger("simulador")


class Interrupcao:
    """Conta os `interromper` recebidos; seguro entre threads."""

    def __init__(self):
        self._trava = threading.Lock()
        self._pedidos = 0

    def ao_receber(self, linha: str) -> None:
        """Callback das saídas: aceita só `{"v":1,"cmd":"interromper"}`, ignora o resto."""
        if not _comando_valido(linha):
            log.warning("comando ignorado (fora do contrato v%d): %s", VERSAO, linha)
            return
        log.info("comando recebido: %s", linha)
        with self._trava:
            self._pedidos += 1

    def novos(self) -> int:
        """Quantos comandos chegaram desde a última chamada."""
        with self._trava:
            pedidos, self._pedidos = self._pedidos, 0
        return pedidos


def _comando_valido(linha: str) -> bool:
    """O Comando do contrato tem valores padrão; aqui `v` e `cmd` precisam vir na linha."""
    try:
        dados = json.loads(linha)
        Comando.model_validate(dados, strict=True)
    except (ValueError, ValidationError):
        return False
    return isinstance(dados, dict) and {"v", "cmd"} <= dados.keys()


class Situacao:
    """O que o robô já enviou no boot atual: posição, última tel e se já parou."""

    def __init__(self):
        self.ultima: tuple[int, Mensagem] | None = None  # (t simulado, mensagem)
        self.tel: Tel | None = None
        self.x = self.y = 0
        self.parado = False

    def registrar(self, t_simulado_ms: int, item: Item) -> None:
        """Atualiza a situação com um item que acabou de sair."""
        if isinstance(item, LinhaCrua):
            return
        if self.ultima is not None and item.boot != self.ultima[1].boot:
            self.tel, self.parado = None, False  # nova tentativa
        self.ultima = (t_simulado_ms, item)
        if hasattr(item, "x"):
            self.x, self.y = item.x, item.y
        if isinstance(item, Tel):
            self.tel = item
        if isinstance(item, Sucesso | Falha) or (
            isinstance(item, HcResultado) and not item.aprovado
        ):
            self.parado = True

    @property
    def pode_parar(self) -> bool:
        """Em health-check ou corrida, com ao menos uma tel já enviada."""
        return self.tel is not None and not self.parado

    def _t_ms(self, t_simulado_ms: int) -> int:
        t_anterior, mensagem = self.ultima
        return mensagem.t_ms + t_simulado_ms - t_anterior

    def confirmacao(self, t_simulado_ms: int) -> Falha:
        """`falha` com origem web na célula atual; o robô passa a ficar parado."""
        _, mensagem = self.ultima
        falha = Falha(
            v=VERSAO,
            boot=mensagem.boot,
            seq=mensagem.seq + 1,
            t_ms=self._t_ms(t_simulado_ms),
            tipo="falha",
            motivo=None,
            origem="web",
            x=self.x,
            y=self.y,
            componente=None,
        )
        self.registrar(t_simulado_ms, falha)
        return falha

    def tel_parada(self, t_simulado_ms: int) -> Tel:
        """Próxima tel com o robô parado em `failed`; a bateria cai 1 mV/s como no gerador."""
        _, mensagem = self.ultima
        t_ms = self._t_ms(t_simulado_ms)
        bat_mv = max(0, self.tel.bat_mv - (t_ms // 1000 - self.tel.t_ms // 1000))
        tel = self.tel.model_copy(
            update={
                "seq": mensagem.seq + 1,
                "t_ms": t_ms,
                "estado": "failed",
                "x": self.x,
                "y": self.y,
                "bat_mv": bat_mv,
                "vel_mm_s": 0,
            }
        )
        self.registrar(t_simulado_ms, tel)
        return tel
