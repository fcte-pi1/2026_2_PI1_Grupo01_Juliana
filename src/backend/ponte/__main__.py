"""Ponto de entrada: `python -m ponte`."""

import argparse
import sys
import threading
from collections.abc import Callable
from pathlib import Path

import httpx

from ponte.auditoria import Auditoria
from ponte.config import ConfigPonte
from ponte.enviadora import Enviadora, criar_cliente
from ponte.fila import Fila
from ponte.leitora import Leitora, Porta, abrir_serial

ESPERA_DAS_THREADS = 2.0


class ErroConfig(Exception):
    """Configuração inválida passada pela linha de comando ou pelo ambiente."""


def montar_config(argv: list[str] | None = None, env_file: str | None = ".env") -> ConfigPonte:
    """Lê o ambiente e o `.env` e aplica as opções; a opção sempre vence a variável."""
    parser = argparse.ArgumentParser(prog="python -m ponte", description=__doc__)
    parser.add_argument("--url", help="URL da API (API_URL)")
    parser.add_argument("--porta", help="porta serial (PONTE_PORTA_SERIAL)")
    parser.add_argument("--fila", type=int, help="máximo de linhas na fila (PONTE_FILA_MAXIMA)")
    parser.add_argument("--log-dir", type=Path, help="pasta do log (PONTE_LOG_DIR)")
    args = parser.parse_args(argv)

    config = ConfigPonte(_env_file=env_file)
    opcoes = {
        "api_url": args.url,
        "porta_serial": args.porta,
        "fila_maxima": args.fila,
        "log_dir": args.log_dir,
    }
    config = config.model_copy(update={k: v for k, v in opcoes.items() if v is not None})
    if config.fila_maxima < 1:
        raise ErroConfig(f"a fila precisa ter pelo menos 1 linha (recebido {config.fila_maxima})")
    return config


class Ponte:
    """Liga a leitora e a enviadora pela fila, cada uma numa thread daemon."""

    def __init__(
        self,
        config: ConfigPonte,
        cliente: httpx.Client | None = None,
        abrir: Callable[[str], Porta] = abrir_serial,
    ) -> None:
        self.config = config
        self.auditoria = Auditoria(config.log_dir)
        self.fila = Fila(config.fila_maxima)
        # Dormir no próprio evento faz o Ctrl+C interromper a espera na hora (D10).
        self._parar = threading.Event()
        self.leitora = Leitora(
            config.porta_serial, self.fila, self.auditoria, abrir=abrir, dormir=self._parar.wait
        )
        self._cliente = criar_cliente(config.api_url) if cliente is None else cliente
        self.enviadora = Enviadora(
            self._cliente, self.fila, self.auditoria, self.leitora.escrever, dormir=self._parar.wait
        )
        self.threads = [
            threading.Thread(target=self.leitora.rodar, args=(self._parar,), daemon=True),
            threading.Thread(target=self.enviadora.rodar, args=(self._parar,), daemon=True),
        ]

    def iniciar(self) -> None:
        for thread in self.threads:
            thread.start()

    def esperar(self) -> None:
        """Bloqueia até o Ctrl+C (KeyboardInterrupt sobe para quem chamou)."""
        while not self._parar.wait(0.5):
            pass

    def encerrar(self) -> None:
        """Para as threads; a fila pendente se perde e fica registrada no log (D10)."""
        self.auditoria.registrar(
            "CONEXAO", f"encerrada, {len(self.fila)} linhas na fila descartadas"
        )
        self._parar.set()
        for thread in self.threads:
            thread.join(ESPERA_DAS_THREADS)
        self._cliente.close()
        self.auditoria.fechar()


def main(argv: list[str] | None = None) -> int:
    try:
        config = montar_config(argv)
    except ErroConfig as erro:
        print(f"ponte: {erro}", file=sys.stderr)
        return 1
    ponte = Ponte(config)
    print(
        f"ponte: porta {config.porta_serial}, API {config.api_url}, log {ponte.auditoria.caminho}",
        file=sys.stderr,
    )
    ponte.iniciar()
    try:
        ponte.esperar()
    except KeyboardInterrupt:
        pass
    finally:
        ponte.encerrar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
