"""Máquina de estados da execução lógica e das tentativas (BACK-03)."""

from __future__ import annotations

import logging
import uuid
from dataclasses import dataclass
from datetime import UTC, datetime

from app.models import ExecucaoLogica, Falha, HealthCheck, Tentativa, TentativaRejeitada
from app.repositories.execucoes import ExecucaoRepository
from app.repositories.repositorio import Repositorio
from app.services.atualizacao_metricas import finalizar_tentativa
from app.services.excecoes_execucao import (
    ExecucaoNaoEncontrada,
    RecusaTentativa,
    TentativaEmAndamento,
    TentativaIndisponivel,
)
from app.services.monitor_conexao import MonitorConexao
from contrato.telemetria import Comando, Mensagem, Sucesso
from contrato.telemetria import Falha as FalhaMsg
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")

ABERTA = ("running",)
TERMINAL_TENTATIVA = ("success", "failed")


@dataclass(frozen=True)
class RetornoTentativa:
    execucao_id: uuid.UUID
    tentativa_id: uuid.UUID
    attempt_index: int


class GerenciadorExecucoes:
    limite_tentativas: int = 3

    def __init__(self, tempo_maximo_execucao_s: int, validade_health_check_s: int = 420) -> None:
        self.tempo_maximo_execucao_s = tempo_maximo_execucao_s
        self.validade_health_check_s = validade_health_check_s
        self._interrupcao_pendente = False
        self._largada_pendente = False
        self._health_check_pendente = False

    def solicitar_health_check(self, sessao: Session) -> datetime:
        """Pede ao robô um novo health-check. Com o robô correndo, o pedido é recusado."""
        if Repositorio(sessao).buscar_tentativa_aberta() is not None:
            raise TentativaEmAndamento()
        self._health_check_pendente = True
        return datetime.now(UTC)

    def confirmar_health_check(self) -> None:
        """O robô começou o health-check: o pedido deixa de ir nas respostas."""
        self._health_check_pendente = False

    def nova_execucao(self, sessao: Session, tipo_labirinto: str) -> RetornoTentativa:
        repo = Repositorio(sessao)
        agora = datetime.now(UTC)
        motivo, health_check = self._conferir_health_check(repo, tipo_labirinto, agora)
        if motivo is not None:
            # sem execução aberta, a recusa não tem onde ser gravada
            raise RecusaTentativa(motivo, agora)

        anterior = repo.buscar_execucao_em_andamento()
        if anterior is not None:
            self._cancelar_execucao(sessao, anterior, agora)

        labirinto_id = repo.labirinto_id_por_tipo(tipo_labirinto)
        if labirinto_id is None:
            raise ValueError(f"labirinto desconhecido: {tipo_labirinto}")

        execucao = ExecucaoLogica(
            execucao_id=uuid.uuid4(),
            labirinto_id=labirinto_id,
            status="em_andamento",
            tentativas_usadas=0,
            iniciada_em=agora,
        )
        repo.salvar(execucao)
        tentativa = self._abrir_tentativa(
            repo,
            execucao,
            health_check,
            attempt_index=1,
            tipo_inicio="nova",
            agora=agora,
        )
        execucao.tentativas_usadas = 1
        return RetornoTentativa(execucao.execucao_id, tentativa.tentativa_id, 1)

    def retomar(self, sessao: Session, execucao_id: uuid.UUID) -> RetornoTentativa:
        repo = Repositorio(sessao)
        agora = datetime.now(UTC)
        execucao = repo.bloquear_execucao(execucao_id)
        if execucao is None:
            raise ExecucaoNaoEncontrada

        if repo.buscar_tentativa_aberta_da_execucao(execucao_id) is not None:
            raise self._recusar(sessao, execucao.execucao_id, "tentativa_aberta", agora)

        if execucao.tentativas_usadas >= self.limite_tentativas:
            raise self._recusar(sessao, execucao.execucao_id, "limite_3", agora)

        if execucao.status != "em_andamento":
            raise self._recusar(sessao, execucao.execucao_id, "execucao_cancelada", agora)

        ultima = execucao.tentativas[-1] if execucao.tentativas else None
        if ultima is None or ultima.status != "failed":
            raise self._recusar(sessao, execucao.execucao_id, "execucao_cancelada", agora)

        motivo, health_check = self._conferir_health_check(repo, execucao.labirinto.tipo, agora)
        if motivo is not None:
            raise self._recusar(sessao, execucao.execucao_id, motivo, agora)

        proximo = execucao.tentativas_usadas + 1
        tentativa = self._abrir_tentativa(
            repo,
            execucao,
            health_check,
            attempt_index=proximo,
            tipo_inicio="retomada",
            agora=agora,
        )
        execucao.tentativas_usadas = proximo
        return RetornoTentativa(execucao.execucao_id, tentativa.tentativa_id, proximo)

    def encerrar_tentativa(
        self,
        sessao: Session,
        execucao_id: uuid.UUID,
        motivo: str,
        observacao: str | None = None,
    ) -> None:
        repo = Repositorio(sessao)
        agora = datetime.now(UTC)
        execucao = repo.bloquear_execucao(execucao_id)
        if execucao is None:
            raise ExecucaoNaoEncontrada
        if execucao.status != "em_andamento":
            raise TentativaIndisponivel

        tentativa = repo.buscar_tentativa_aberta_da_execucao(execucao_id)
        if tentativa is None:
            raise TentativaIndisponivel

        celula = self._ultima_celula(repo, tentativa)
        self._registrar_falha(
            repo,
            tentativa,
            motivo=motivo,
            origem="encerrado_operador",
            x=celula[0],
            y=celula[1],
            componente=None,
            observacao=observacao,
            agora=agora,
        )
        self._fechar_tentativa_falha(sessao, tentativa, execucao, agora)
        self._interrupcao_pendente = True

    def aplicar_evento(
        self,
        sessao: Session,
        tentativa: Tentativa | None,
        mensagem: Mensagem,
        *,
        momento: datetime | None = None,
    ) -> None:
        logger.info(
            "evento %s recebido (tentativa %s)",
            mensagem.tipo,
            tentativa.tentativa_id if tentativa is not None else None,
        )
        if isinstance(mensagem, FalhaMsg) and mensagem.origem == "web":
            self._confirmar_interrupcao(tentativa)
            return

        if tentativa is None:
            return

        repo = Repositorio(sessao)
        agora = momento or datetime.now(UTC)
        execucao = tentativa.execucao

        if isinstance(mensagem, FalhaMsg):
            self._aplicar_falha_telemetria(sessao, repo, tentativa, execucao, mensagem, agora)
        elif isinstance(mensagem, Sucesso):
            self._aplicar_sucesso(sessao, tentativa, execucao, agora)

    def verificar_monitores(
        self, sessao: Session, monitor: MonitorConexao, agora: datetime | None = None
    ) -> bool:
        """Cancela a tentativa aberta por link ou tempo. Devolve se cancelou."""
        agora = agora or datetime.now(UTC)
        repo = Repositorio(sessao)
        tentativa = repo.buscar_tentativa_aberta()
        if tentativa is None:
            return False
        execucao = tentativa.execucao
        if execucao.status != "em_andamento":
            return False

        sem_sinal = monitor.segundos_sem_sinal(agora, inicio=tentativa.iniciada_em)
        if sem_sinal is not None and sem_sinal >= monitor.limite_sem_sinal_s:
            celula = self._ultima_celula(repo, tentativa)
            self._registrar_falha(
                repo,
                tentativa,
                motivo="link_lost",
                origem="automatica",
                x=celula[0],
                y=celula[1],
                agora=agora,
            )
            self._fechar_tentativa_falha(sessao, tentativa, execucao, agora)
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
            finalizar_tentativa(sessao, tentativa, execucao, agora)
            self._interrupcao_pendente = False
            return True

        decorrido = (agora - execucao.iniciada_em).total_seconds()
        if decorrido >= self.tempo_maximo_execucao_s:
            celula = self._ultima_celula(repo, tentativa)
            self._registrar_falha(
                repo,
                tentativa,
                motivo="time_exceeded",
                origem="automatica",
                x=celula[0],
                y=celula[1],
                agora=agora,
            )
            self._fechar_tentativa_falha(sessao, tentativa, execucao, agora)
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
            finalizar_tentativa(sessao, tentativa, execucao, agora)
            self._interrupcao_pendente = False
            return True
        return False

    def comandos_pendentes(self) -> list[Comando]:
        comandos = []
        if self._interrupcao_pendente:
            comandos.append(Comando(cmd="interromper"))
        if self._largada_pendente:
            comandos.append(Comando(cmd="largar"))
        if self._health_check_pendente:
            comandos.append(Comando(cmd="health_check"))
        return comandos

    def confirmar_largada(self) -> None:
        """O robô já está correndo: o `largar` deixa de ir nas respostas."""
        self._largada_pendente = False

    def _conferir_health_check(
        self, repo: Repositorio, tipo_labirinto: str, agora: datetime
    ) -> tuple[str | None, HealthCheck | None]:
        """Devolve o motivo da recusa, ou None e o health-check que libera a largada."""
        health_check = repo.ultimo_health_check_concluido()
        if health_check is None:
            return "health_check_ausente", None
        if not health_check.aprovado:
            return "health_check_reprovado", None
        idade = (agora - health_check.concluido_em).total_seconds()
        if idade > self.validade_health_check_s:
            return "health_check_expirado", None
        if health_check.tipo_dip != tipo_labirinto:
            return "dip_divergente", None
        return None, health_check

    def _aplicar_falha_telemetria(
        self,
        sessao: Session,
        repo: Repositorio,
        tentativa: Tentativa,
        execucao: ExecucaoLogica,
        mensagem: FalhaMsg,
        agora: datetime,
    ) -> None:
        if tentativa.status not in ABERTA:
            return

        origem_db = "encerrado_operador" if mensagem.origem == "boot" else "automatica"
        motivo = mensagem.motivo or "collision"
        self._registrar_falha(
            repo,
            tentativa,
            motivo=motivo,
            origem=origem_db,
            x=mensagem.x,
            y=mensagem.y,
            componente=mensagem.componente,
            agora=agora,
        )
        self._fechar_tentativa_falha(sessao, tentativa, execucao, agora)
        if mensagem.origem == "boot":
            self._interrupcao_pendente = False

    def _aplicar_sucesso(
        self,
        sessao: Session,
        tentativa: Tentativa,
        execucao: ExecucaoLogica,
        agora: datetime,
    ) -> None:
        if tentativa.status != "running":
            return
        tentativa.status = "success"
        tentativa.encerrada_em = agora
        execucao.status = "concluida"
        execucao.encerrada_em = agora
        finalizar_tentativa(sessao, tentativa, execucao, agora)
        self._interrupcao_pendente = False
        self._largada_pendente = False

    def _confirmar_interrupcao(self, tentativa: Tentativa | None) -> None:
        if tentativa is not None and tentativa.status in ABERTA:
            return
        self._interrupcao_pendente = False

    def _fechar_tentativa_falha(
        self,
        sessao: Session,
        tentativa: Tentativa,
        execucao: ExecucaoLogica,
        agora: datetime,
    ) -> None:
        tentativa.status = "failed"
        tentativa.encerrada_em = agora
        self._largada_pendente = False
        if tentativa.attempt_index >= self.limite_tentativas:
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
        finalizar_tentativa(sessao, tentativa, execucao, agora)

    def _cancelar_execucao(
        self, sessao: Session, execucao: ExecucaoLogica, agora: datetime
    ) -> None:
        repo = Repositorio(sessao)
        aberta = repo.buscar_tentativa_aberta_da_execucao(execucao.execucao_id)
        if aberta is not None:
            aberta.status = "failed"
            aberta.encerrada_em = agora
            self._largada_pendente = False
        execucao.status = "cancelada"
        execucao.encerrada_em = agora
        if aberta is not None:
            finalizar_tentativa(sessao, aberta, execucao, agora)

    def _abrir_tentativa(
        self,
        repo: Repositorio,
        execucao: ExecucaoLogica,
        health_check: HealthCheck,
        *,
        attempt_index: int,
        tipo_inicio: str,
        agora: datetime,
    ) -> Tentativa:
        """Abre a tentativa já em running: a largada é o clique do operador."""
        tentativa = Tentativa(
            tentativa_id=uuid.uuid4(),
            execucao_id=execucao.execucao_id,
            attempt_index=attempt_index,
            status="running",
            tipo_inicio=tipo_inicio,
            tipo_dip=health_check.tipo_dip,
            hc_id=health_check.hc_id,
            iniciada_em=agora,
        )
        repo.salvar(tentativa)
        self._interrupcao_pendente = False
        self._largada_pendente = True
        # a largada vence um pedido de health-check que o robô ainda não atendeu
        self._health_check_pendente = False
        return tentativa

    def _registrar_falha(
        self,
        repo: Repositorio,
        tentativa: Tentativa,
        *,
        motivo: str,
        origem: str,
        x: int,
        y: int,
        componente: str | None = None,
        observacao: str | None = None,
        agora: datetime,
    ) -> None:
        if tentativa.falha is not None:
            return
        repo.salvar(
            Falha(
                tentativa_id=tentativa.tentativa_id,
                motivo=motivo,
                origem=origem,
                celula_x=x,
                celula_y=y,
                componente=componente,
                observacao=observacao,
                momento_falha=agora,
            )
        )
        repo._session.refresh(tentativa, attribute_names=["falha"])

    def _ultima_celula(self, repo: Repositorio, tentativa: Tentativa) -> tuple[int, int]:
        leituras = ExecucaoRepository(repo._session).listar_leituras(tentativa.tentativa_id)
        if leituras:
            ultima = leituras[-1]
            if ultima.x is not None and ultima.y is not None:
                return ultima.x, ultima.y
        return 0, 0

    def _recusar(
        self, sessao: Session, execucao_id: uuid.UUID, motivo: str, agora: datetime
    ) -> RecusaTentativa:
        Repositorio(sessao).salvar(
            TentativaRejeitada(execucao_id=execucao_id, motivo=motivo, ocorrida_em=agora)
        )
        return RecusaTentativa(motivo, agora)
