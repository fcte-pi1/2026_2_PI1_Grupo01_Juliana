"""Ingestão das linhas do robô recebidas pelo POST /telemetria (BACK-02)."""

import logging
import threading
from datetime import datetime

from app.models import LeituraTelemetria, Tentativa
from app.repositories.repositorio import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from contrato.telemetria import (
    TAMANHO_MAXIMO,
    Deduplicador,
    EntradaPonte,
    Falha,
    LinhaInvalida,
    Mensagem,
    Passo,
    RelogioDoRobo,
    RespostaPonte,
    Sucesso,
    Tel,
    ler_linha,
)
from sqlalchemy.orm import Session

logger = logging.getLogger("app.telemetria")

COM_CELULA = (Tel, Passo, Falha, Sucesso)


class IngestaoTelemetria:
    """Valida a linha com o contrato v1, associa à tentativa aberta e grava."""

    def __init__(
        self, deduplicador: Deduplicador, relogio: RelogioDoRobo, monitor: MonitorConexao
    ) -> None:
        self.deduplicador = deduplicador
        self.relogio = relogio
        self.monitor = monitor
        # a rota síncrona roda no threadpool: protege o max+1 e o estado em memória
        self._trava = threading.Lock()

    def processar(
        self, sessao: Session, entrada: EntradaPonte, gerenciador: GerenciadorExecucoes
    ) -> RespostaPonte:
        """Processa uma linha. Levanta `LinhaInvalida` se ela violar o contrato."""
        with self._trava:
            self._processar(sessao, entrada)
        return RespostaPonte(comandos=gerenciador.comandos_pendentes())

    def _processar(self, sessao: Session, entrada: EntradaPonte) -> None:
        try:
            mensagem = ler_linha(entrada.linha)
        except LinhaInvalida as erro:
            logger.warning(
                "linha inválida descartada: %s | linha: %r", erro, entrada.linha[:TAMANHO_MAXIMO]
            )
            raise

        self.monitor.registrar_mensagem(entrada.recebido_em)
        enviado_em = self.relogio.enviado_em(mensagem, entrada.recebido_em)

        tentativa = Repositorio(sessao).buscar_tentativa_aberta()
        if tentativa is None:
            logger.info("%s descartada: nenhuma tentativa aberta", mensagem.tipo)
            return

        self._conferir_limites(mensagem, tentativa)

        if self.deduplicador.eh_repetida(mensagem):
            logger.info(
                "%s repetida descartada (boot %d, seq %d)",
                mensagem.tipo,
                mensagem.boot,
                mensagem.seq,
            )
            return

        try:
            if isinstance(mensagem, Tel):
                self._gravar_tel(sessao, tentativa, mensagem, enviado_em, entrada.recebido_em)
            sessao.commit()
        except Exception:
            sessao.rollback()
            raise
        self.deduplicador.confirmar(mensagem)

    def _conferir_limites(self, mensagem: Mensagem, tentativa: Tentativa) -> None:
        """A célula precisa caber no labirinto da execução, em qualquer orientação (D4)."""
        if not isinstance(mensagem, COM_CELULA):
            return
        labirinto = tentativa.execucao.labirinto
        lado_longo = max(labirinto.largura, labirinto.altura)
        lado_curto = min(labirinto.largura, labirinto.altura)
        x, y = mensagem.x, mensagem.y
        if max(x, y) < lado_longo and min(x, y) < lado_curto:
            return
        motivo = f"célula ({x}, {y}) fora do labirinto {labirinto.tipo}"
        logger.warning("linha inválida descartada: %s", motivo)
        raise LinhaInvalida(motivo)

    def _gravar_tel(
        self,
        sessao: Session,
        tentativa: Tentativa,
        tel: Tel,
        enviado_em: datetime,
        recebido_em: datetime,
    ) -> None:
        ordem = TelemetriaRepository(sessao).proxima_ordem_leitura(tentativa.tentativa_id)
        leitura = LeituraTelemetria(
            tentativa_id=tentativa.tentativa_id,
            ordem=ordem,
            fase=tentativa.status,
            x=tel.x,
            y=tel.y,
            bateria=tel.bat_mv,
            velocidade=tel.vel_mm_s,
            enviado_em=enviado_em,
            recebido_em=recebido_em,
        )
        Repositorio(sessao).salvar(leitura)
