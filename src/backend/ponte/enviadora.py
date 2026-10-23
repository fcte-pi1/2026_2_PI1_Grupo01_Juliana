"""Enviadora: POST /telemetria em ordem e escrita dos comandos na serial (D4, D7)."""

import json
import threading
import time
from collections.abc import Callable
from typing import Any

import httpx
from contrato.telemetria import Comando
from pydantic import ValidationError

from ponte.auditoria import Auditoria
from ponte.espera import Espera
from ponte.fila import Fila, Item

TIMEOUT_DA_API = 2.0


def criar_cliente(api_url: str) -> httpx.Client:
    return httpx.Client(base_url=api_url, timeout=TIMEOUT_DA_API)


class Enviadora:
    """Envia o primeiro item da fila e só o remove quando a API responde 2xx ou 4xx."""

    def __init__(
        self,
        cliente: httpx.Client,
        fila: Fila,
        auditoria: Auditoria,
        escrever: Callable[[str], bool],
        dormir: Callable[[float], None] = time.sleep,
    ) -> None:
        self._cliente = cliente
        self._fila = fila
        self._auditoria = auditoria
        self._escrever = escrever
        self._dormir = dormir
        self._espera = Espera()

    def rodar(self, parar: threading.Event) -> None:
        """Envia os itens da fila até `parar` ser sinalizado."""
        while not parar.is_set():
            try:
                self.enviar_primeiro(timeout=0.2, parar=parar)
            except Exception as erro:
                self._auditoria.registrar("API", f"erro inesperado {type(erro).__name__} {erro}")
                if not parar.is_set():
                    self._dormir(self._espera.proxima())

    def enviar_primeiro(self, timeout: float, parar: threading.Event | None = None) -> None:
        """Uma tentativa com o item mais antigo; na falha, dorme a próxima espera."""
        item = self._fila.primeiro(timeout)
        if item is None:
            return
        comandos = self._enviar(item)
        if comandos is None:
            if parar is None or not parar.is_set():
                self._dormir(self._espera.proxima())
            return
        # Se o item foi descartado durante o envio, remover_primeiro() não remove outro.
        self._fila.remover_primeiro()
        self._espera.zerar()
        for comando in comandos:
            self._escrever_comando(comando)

    def _enviar(self, item: Item) -> list[Any] | None:
        """Devolve os comandos da resposta, ou None quando o item precisa ser reenviado."""
        linha, recebido_em = item
        corpo = {"linha": linha, "recebido_em": recebido_em.isoformat()}
        try:
            resposta = self._cliente.post("/telemetria", json=corpo)
        except httpx.TransportError as erro:
            self._auditoria.registrar("API", f"falhou {type(erro).__name__} {erro}")
            return None
        status = resposta.status_code
        if status >= 500:
            self._auditoria.registrar("API", f"falhou status {status}")
            return None
        comandos = _ler_comandos(resposta) if status < 300 or status == 422 else []
        self._auditoria.registrar("API", f"{status} comandos={len(comandos)}")
        return comandos

    def _escrever_comando(self, item: Any) -> None:
        try:
            if not isinstance(item, dict) or not {"v", "cmd"} <= item.keys():
                raise ValueError("comando sem v ou cmd")
            comando = Comando.model_validate(item, strict=True)
        except (ValidationError, ValueError):
            recusado = json.dumps(item, ensure_ascii=False, separators=(",", ":"))
            self._auditoria.registrar("API", f"comando recusado {recusado}")
            return
        texto = comando.model_dump_json()
        if self._escrever(texto + "\n"):
            self._auditoria.registrar("TX", texto)


def _ler_comandos(resposta: httpx.Response) -> list[Any]:
    """A lista `comandos` do corpo, ou vazia se o corpo não a tiver."""
    try:
        corpo = resposta.json()
    except ValueError:
        return []
    if not isinstance(corpo, dict) or not isinstance(corpo.get("comandos"), list):
        return []
    return corpo["comandos"]
