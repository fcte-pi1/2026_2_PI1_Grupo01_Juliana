"""Ingestão das linhas do robô recebidas pelo POST /telemetria (BACK-02)."""

import logging

from app.repositories.repositorio import Repositorio
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from contrato.telemetria import (
    TAMANHO_MAXIMO,
    Deduplicador,
    EntradaPonte,
    LinhaInvalida,
    RelogioDoRobo,
    RespostaPonte,
    ler_linha,
)
from sqlalchemy.orm import Session

logger = logging.getLogger("app.telemetria")


class IngestaoTelemetria:
    """Valida a linha com o contrato v1 e a associa à tentativa aberta."""

    def __init__(self, deduplicador: Deduplicador, relogio: RelogioDoRobo) -> None:
        self.deduplicador = deduplicador
        self.relogio = relogio

    def processar(
        self, sessao: Session, entrada: EntradaPonte, gerenciador: GerenciadorExecucoes
    ) -> RespostaPonte:
        """Processa uma linha. Levanta `LinhaInvalida` se ela violar o contrato."""
        try:
            mensagem = ler_linha(entrada.linha)
        except LinhaInvalida as erro:
            logger.warning(
                "linha inválida descartada: %s | linha: %r", erro, entrada.linha[:TAMANHO_MAXIMO]
            )
            raise

        tentativa = Repositorio(sessao).buscar_tentativa_aberta()
        if tentativa is None:
            logger.info("%s descartada: nenhuma tentativa aberta", mensagem.tipo)

        return RespostaPonte(comandos=gerenciador.comandos_pendentes())
