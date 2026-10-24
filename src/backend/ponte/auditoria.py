"""Log de auditoria da ponte: um arquivo de texto por execução (RNF-B06)."""

import threading
from collections.abc import Callable
from datetime import datetime
from pathlib import Path


def _agora() -> datetime:
    return datetime.now().astimezone()


class Auditoria:
    """Escreve `<hora ISO> <TIPO> <conteudo>` por linha, com flush, segura entre threads.

    Tipos usados: RX, TX, API, CONEXAO e FILA.
    """

    def __init__(self, log_dir: Path, relogio: Callable[[], datetime] = _agora) -> None:
        self._relogio = relogio
        self._trava = threading.Lock()
        log_dir = Path(log_dir)
        log_dir.mkdir(parents=True, exist_ok=True)
        self.caminho = log_dir / f"ponte-{relogio():%Y%m%d-%H%M%S}.log"
        self._arquivo = self.caminho.open("a", encoding="utf-8")

    def registrar(self, tipo: str, conteudo: str) -> None:
        hora = self._relogio().isoformat(timespec="milliseconds")
        with self._trava:
            self._arquivo.write(f"{hora} {tipo} {conteudo}\n")
            self._arquivo.flush()

    def fechar(self) -> None:
        with self._trava:
            self._arquivo.close()

    def __enter__(self) -> "Auditoria":
        return self

    def __exit__(self, *_: object) -> None:
        self.fechar()
