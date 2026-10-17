"""Ingestão das linhas do robô recebidas pelo POST /telemetria (BACK-02)."""

import logging
import threading
import uuid
from collections.abc import Callable
from datetime import UTC, datetime
from decimal import Decimal

from app.models import HealthCheck, HealthCheckItem, LeituraTelemetria, PassoTrajeto, Tentativa
from app.repositories.repositorio import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from app.services.atualizacao_metricas import recalcular_parciais
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from app.services.vigilancia_monitores import gravar_se_cancelou
from contrato.telemetria import (
    TAMANHO_MAXIMO,
    Deduplicador,
    EntradaPonte,
    Falha,
    HcItem,
    HcResultado,
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
# repassados ao GerenciadorExecucoes, que cuida das transições (D2)
EVENTOS = (Falha, Sucesso)
# o health-check é pré-requisito da largada: chega sem tentativa aberta
HEALTH_CHECK = (HcItem, HcResultado)


class IngestaoTelemetria:
    """Valida a linha com o contrato v1, associa à tentativa aberta e grava."""

    def __init__(
        self,
        deduplicador: Deduplicador,
        relogio: RelogioDoRobo,
        monitor: MonitorConexao,
        relogio_backend: Callable[[], datetime] | None = None,
    ) -> None:
        self.deduplicador = deduplicador
        self.relogio = relogio
        self.monitor = monitor
        self.relogio_backend = relogio_backend or (lambda: datetime.now(UTC))
        # a rota síncrona roda no threadpool: protege o max+1 e o estado em memória
        # a varredura de link/tempo usa a mesma trava
        self._trava = threading.Lock()

    @property
    def trava(self) -> threading.Lock:
        return self._trava

    def processar(
        self, sessao: Session, entrada: EntradaPonte, gerenciador: GerenciadorExecucoes
    ) -> RespostaPonte:
        """Processa uma linha. Levanta `LinhaInvalida` se ela violar o contrato."""
        with self._trava:
            self._processar(sessao, entrada, gerenciador)
        return RespostaPonte(comandos=gerenciador.comandos_pendentes())

    def _processar(
        self, sessao: Session, entrada: EntradaPonte, gerenciador: GerenciadorExecucoes
    ) -> None:
        try:
            mensagem = ler_linha(entrada.linha)
        except LinhaInvalida as erro:
            logger.warning(
                "linha inválida descartada: %s | linha: %r", erro, entrada.linha[:TAMANHO_MAXIMO]
            )
            raise

        agora_backend = self.relogio_backend()
        # antes de marcar esta linha como sinal: o silêncio é o intervalo anterior
        gravar_se_cancelou(sessao, gerenciador, self.monitor)
        # relógio do servidor: o recebido_em de uma linha reenviada pela ponte é antigo
        self.monitor.registrar_mensagem()

        tentativa = Repositorio(sessao).buscar_tentativa_aberta()
        if tentativa is not None:
            self._conferir_limites(mensagem, tentativa)
        # a interrupção pela web pode chegar depois de a tentativa fechar
        elif not isinstance(mensagem, HEALTH_CHECK) and not (
            isinstance(mensagem, Falha) and mensagem.origem == "web"
        ):
            logger.info("%s descartada: nenhuma tentativa aberta", mensagem.tipo)
            return

        if self.deduplicador.eh_repetida(mensagem):
            logger.info(
                "%s repetida descartada (boot %d, seq %d)",
                mensagem.tipo,
                mensagem.boot,
                mensagem.seq,
            )
            return

        enviado_em = self.relogio.calcular(mensagem, entrada.recebido_em)
        try:
            if isinstance(mensagem, Tel):
                self._gravar_tel(
                    sessao,
                    tentativa,
                    mensagem,
                    enviado_em,
                    entrada.recebido_em,
                    agora_backend,
                )
            elif isinstance(mensagem, HcItem):
                self._gravar_hc_item(sessao, mensagem, enviado_em, agora_backend)
            elif isinstance(mensagem, HcResultado):
                self._gravar_hc_resultado(sessao, mensagem, agora_backend)
            elif isinstance(mensagem, Passo):
                self._gravar_passo(sessao, tentativa, mensagem, enviado_em, agora_backend)
            elif isinstance(mensagem, EVENTOS):
                gerenciador.aplicar_evento(sessao, tentativa, mensagem, momento=agora_backend)
            if isinstance(mensagem, Passo) or (
                isinstance(mensagem, Tel) and mensagem.estado == "running"
            ):
                gerenciador.confirmar_largada()
            if isinstance(mensagem, HEALTH_CHECK):
                gerenciador.confirmar_health_check()
            sessao.commit()
        except Exception:
            sessao.rollback()
            raise
        # a âncora do boot só é fixada por uma mensagem aceita
        self.relogio.fixar(mensagem, entrada.recebido_em)
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
        agora_backend: datetime,
    ) -> None:
        ordem = TelemetriaRepository(sessao).proxima_ordem_leitura(tentativa.tentativa_id)
        leitura = LeituraTelemetria(
            tentativa_id=tentativa.tentativa_id,
            ordem=ordem,
            fase=tentativa.status,
            x=tel.x,
            y=tel.y,
            bateria=Decimal(tel.bat_mv),
            velocidade=tel.vel_mm_s,
            rumo=tel.rumo,
            enviado_em=enviado_em,
            recebido_em=recebido_em,
        )
        Repositorio(sessao).salvar(leitura)
        recalcular_parciais(
            sessao,
            tentativa,
            tentativa.execucao,
            agora_backend,
            bateria_mv_atual=tel.bat_mv,
        )

    def _health_check_do_boot(self, sessao: Session, boot: int, agora: datetime) -> HealthCheck:
        """O health-check em andamento deste boot, ou um novo."""
        repositorio = Repositorio(sessao)
        health_check = repositorio.health_check_em_andamento(boot)
        if health_check is None:
            health_check = HealthCheck(hc_id=uuid.uuid4(), boot=boot, iniciado_em=agora)
            repositorio.salvar(health_check)
        return health_check

    def _gravar_hc_item(
        self, sessao: Session, item: HcItem, enviado_em: datetime, agora: datetime
    ) -> None:
        health_check = self._health_check_do_boot(sessao, item.boot, agora)
        registro = HealthCheckItem(
            hc_id=health_check.hc_id,
            componente=item.componente,
            aprovado=item.aprovado,
            valor_lido=item.valor,
            verificado_em=enviado_em,
        )
        Repositorio(sessao).salvar(registro)

    def _gravar_hc_resultado(
        self, sessao: Session, resultado: HcResultado, agora: datetime
    ) -> None:
        # concluido_em no relógio do backend: é com ele que a validade é conferida
        health_check = self._health_check_do_boot(sessao, resultado.boot, agora)
        health_check.aprovado = resultado.aprovado
        health_check.tipo_dip = resultado.tipo_dip
        health_check.concluido_em = agora
        logger.info(
            "health-check %s (DIP %s)",
            "aprovado" if resultado.aprovado else "reprovado",
            resultado.tipo_dip,
        )

    def _gravar_passo(
        self,
        sessao: Session,
        tentativa: Tentativa,
        passo: Passo,
        enviado_em: datetime,
        agora_backend: datetime,
    ) -> None:
        """Grava o passo (D5, D6) e as paredes da célula (D7), na mesma transação."""
        repositorio = TelemetriaRepository(sessao)
        ultimo = repositorio.ultimo_passo(tentativa.execucao_id)
        # o último passo da execução é de outra tentativa: este é o 1º desta
        primeiro_da_tentativa = ultimo is None or ultimo.tentativa_id != tentativa.tentativa_id
        if not primeiro_da_tentativa and (ultimo.x, ultimo.y) == (passo.x, passo.y):
            # releitura da mesma célula na mesma tentativa: só as paredes mudam
            ultimo.paredes_mask = passo.paredes
        else:
            # a retomada começa na célula da falha e ganha o próprio passo
            retomada = tentativa.tipo_inicio == "retomada" and primeiro_da_tentativa
            registro = PassoTrajeto(
                tentativa_id=tentativa.tentativa_id,
                execucao_id=tentativa.execucao_id,
                seq=ultimo.seq + 1 if ultimo is not None else 1,
                x=passo.x,
                y=passo.y,
                paredes_mask=passo.paredes,
                retomada=retomada,
                entrou_em=enviado_em,
            )
            Repositorio(sessao).salvar(registro)
        repositorio.gravar_paredes(
            tentativa.execucao_id, passo.x, passo.y, passo.paredes, enviado_em
        )
        recalcular_parciais(sessao, tentativa, tentativa.execucao, agora_backend)
