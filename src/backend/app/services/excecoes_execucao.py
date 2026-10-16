"""Erros de domínio da execução lógica (BACK-03)."""

from datetime import datetime


class RecusaTentativa(Exception):
    def __init__(self, motivo: str, ocorrida_em: datetime) -> None:
        self.motivo = motivo
        self.ocorrida_em = ocorrida_em
        super().__init__(motivo)


class ExecucaoNaoEncontrada(Exception):
    pass


class TentativaIndisponivel(Exception):
    """Encerrar ou ação similar sem tentativa aberta na execução."""

    pass
