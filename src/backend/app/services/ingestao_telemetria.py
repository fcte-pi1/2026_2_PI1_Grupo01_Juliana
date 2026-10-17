"""Ingestão das linhas do robô recebidas pelo POST /telemetria (BACK-02)."""

import logging
import threading
from datetime import datetime
from decimal import Decimal

from app.models import HealthCheckItem, LeituraTelemetria, PassoTrajeto, Tentativa
from app.repositories.repositorio import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from app.services.atualizacao_metricas import recalcular_parciais
from app.services.fabrica_eventos import OrigemTelemetria, criar_eventos
from app.services.gerenciador_execucoes import GerenciadorExecucoes
from app.services.monitor_conexao import MonitorConexao
from app.services.publicacao_eventos import publicar_apos_commit
from app.services.publicador_sse import EventoSSE, PublicadorSSE
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
EVENTOS = (HcResultado, Falha, Sucesso)


class IngestaoTelemetria:
    """Valida a linha com o contrato v1, associa à tentativa aberta e grava."""

    def __init__(
        self,
        deduplicador: Deduplicador,
        relogio: RelogioDoRobo,
        monitor: MonitorConexao,
        publicador: PublicadorSSE | None = None,
    ) -> None:
        self.deduplicador = deduplicador
        self.relogio = relogio
        self.monitor = monitor
        self.publicador = publicador
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

        # antes de marcar esta linha como sinal: o silêncio é o intervalo anterior
        resultado_monitor = gerenciador.verificar_monitores(sessao, self.monitor)
        sessao.commit()
        if resultado_monitor:
            publicar_apos_commit(
                self.publicador, resultado_monitor.execucao_id, resultado_monitor.eventos
            )
            return
        # relógio do servidor: o recebido_em de uma linha reenviada pela ponte é antigo
        self.monitor.registrar_mensagem()

        tentativa = Repositorio(sessao).buscar_tentativa_aberta()
        if tentativa is None:
            # a interrupção pela web pode chegar depois de a tentativa fechar
            if not (isinstance(mensagem, Falha) and mensagem.origem == "web"):
                logger.info("%s descartada: nenhuma tentativa aberta", mensagem.tipo)
                return
        else:
            self._conferir_limites(mensagem, tentativa)
            if isinstance(mensagem, HcItem) and tentativa.status != "health-check":
                logger.info("hc_item descartado: tentativa em %s", tentativa.status)
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
        # lido antes do COMMIT: depois dele o atributo expira e custaria um SELECT
        execucao_id = str(tentativa.execucao_id) if tentativa is not None else None
        eventos: list[EventoSSE] = []
        try:
            if isinstance(mensagem, Tel):
                eventos = self._gravar_tel(
                    sessao, tentativa, mensagem, enviado_em, entrada.recebido_em
                )
            elif isinstance(mensagem, HcItem):
                eventos = self._gravar_hc_item(sessao, tentativa, mensagem, enviado_em)
            elif isinstance(mensagem, Passo):
                eventos = self._gravar_passo(
                    sessao, tentativa, mensagem, enviado_em, entrada.recebido_em
                )
            elif isinstance(mensagem, EVENTOS):
                eventos = (
                    gerenciador.aplicar_evento(
                        sessao, tentativa, mensagem, momento=entrada.recebido_em
                    )
                    or []
                )
            sessao.commit()
        except Exception:
            sessao.rollback()
            raise
        # a âncora do boot só é fixada por uma mensagem aceita
        self.relogio.fixar(mensagem, entrada.recebido_em)
        self.deduplicador.confirmar(mensagem)
        # só depois do COMMIT (4.4, "Trajeto em tempo real"). Ainda dentro da trava,
        # para a ordem na fila do cliente ser a ordem dos commits. A linha já está
        # gravada, então nada daqui para frente pode falhar a requisição: a
        # ingestão não para por causa de um navegador (RNF-B07) e o cliente
        # recupera o que perdeu pelo reenvio.
        if execucao_id is not None:
            publicar_apos_commit(self.publicador, execucao_id, eventos)

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
    ) -> list[EventoSSE]:
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
        # rumo e eixo_longo vêm da mensagem: existem no contrato serial e no evento
        # SSE, mas não têm coluna em LEITURA_TELEMETRIA
        recalcular_parciais(
            sessao,
            tentativa,
            tentativa.execucao,
            recebido_em,
            bateria_mv_atual=tel.bat_mv,
        )
        return criar_eventos(OrigemTelemetria(tel=tel, ordem=ordem, enviado_em=enviado_em))

    def _gravar_hc_item(
        self, sessao: Session, tentativa: Tentativa, item: HcItem, enviado_em: datetime
    ) -> list[EventoSSE]:
        registro = HealthCheckItem(
            tentativa_id=tentativa.tentativa_id,
            componente=item.componente,
            aprovado=item.aprovado,
            valor_lido=item.valor,
            verificado_em=enviado_em,
        )
        Repositorio(sessao).salvar(registro)
        return criar_eventos(registro)

    def _gravar_passo(
        self,
        sessao: Session,
        tentativa: Tentativa,
        passo: Passo,
        enviado_em: datetime,
        recebido_em: datetime,
    ) -> list[EventoSSE]:
        """Grava o passo (D5, D6) e as paredes da célula (D7), na mesma transação."""
        repositorio = TelemetriaRepository(sessao)
        ultimo = repositorio.ultimo_passo(tentativa.execucao_id)
        # o último passo da execução é de outra tentativa: este é o 1º desta
        primeiro_da_tentativa = ultimo is None or ultimo.tentativa_id != tentativa.tentativa_id
        if not primeiro_da_tentativa and (ultimo.x, ultimo.y) == (passo.x, passo.y):
            # releitura da mesma célula na mesma tentativa: só as paredes mudam
            ultimo.paredes_mask = passo.paredes
            # republica o mesmo passo_id de propósito: a parede é nova e o
            # desenho precisa dela
            eventos = criar_eventos(ultimo)
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
            eventos = criar_eventos(registro)
        repositorio.gravar_paredes(
            tentativa.execucao_id, passo.x, passo.y, passo.paredes, enviado_em
        )
        recalcular_parciais(sessao, tentativa, tentativa.execucao, recebido_em)
        return eventos
