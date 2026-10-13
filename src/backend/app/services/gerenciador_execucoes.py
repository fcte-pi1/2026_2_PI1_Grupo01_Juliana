"""Máquina de estados da execução lógica e das tentativas (BACK-03)."""

from __future__ import annotations

import logging
import uuid
from dataclasses import dataclass
from datetime import UTC, datetime
from decimal import Decimal

from app.models import ExecucaoLogica, Falha, Tentativa, TentativaRejeitada
from app.repositories.execucoes import ExecucaoRepository
from app.repositories.repositorio import Repositorio
from app.services.excecoes_execucao import (
    ExecucaoNaoEncontrada,
    RecusaTentativa,
    TentativaIndisponivel,
)
from app.services.monitor_conexao import MonitorConexao
from contrato.telemetria import Comando, Falha as FalhaMsg, HcResultado, Mensagem, Sucesso
from sqlalchemy.orm import Session

logger = logging.getLogger("app.gerenciador")

ABERTA = ("health-check", "running")
TERMINAL_TENTATIVA = ("success", "failed")


@dataclass(frozen=True)
class RetornoTentativa:
    execucao_id: uuid.UUID
    tentativa_id: uuid.UUID
    attempt_index: int


class GerenciadorExecucoes:
    limite_tentativas: int = 3

    def __init__(self, tempo_maximo_execucao_s: int) -> None:
        self.tempo_maximo_execucao_s = tempo_maximo_execucao_s
        self._interrupcao_pendente = False

    def nova_execucao(self, sessao: Session, tipo_labirinto: str) -> RetornoTentativa:
        repo = Repositorio(sessao)
        agora = datetime.now(UTC)
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
        tentativa = self._abrir_tentativa(repo, execucao, attempt_index=1, tipo_inicio="nova", agora=agora)
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

        proximo = execucao.tentativas_usadas + 1
        tentativa = self._abrir_tentativa(
            repo, execucao, attempt_index=proximo, tipo_inicio="retomada", agora=agora
        )
        execucao.tentativas_usadas = proximo
        self._interrupcao_pendente = False
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
        self._fechar_tentativa_falha(tentativa, execucao, agora)
        self._interrupcao_pendente = True

    def aplicar_evento(
        self, sessao: Session, tentativa: Tentativa | None, mensagem: Mensagem
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
        agora = datetime.now(UTC)
        execucao = tentativa.execucao

        if isinstance(mensagem, HcResultado):
            self._aplicar_hc_resultado(repo, tentativa, execucao, mensagem, agora)
        elif isinstance(mensagem, FalhaMsg):
            self._aplicar_falha_telemetria(repo, tentativa, execucao, mensagem, agora)
        elif isinstance(mensagem, Sucesso):
            self._aplicar_sucesso(tentativa, execucao, mensagem, agora)

    def verificar_monitores(
        self, sessao: Session, monitor: MonitorConexao, agora: datetime | None = None
    ) -> None:
        agora = agora or datetime.now(UTC)
        repo = Repositorio(sessao)
        tentativa = repo.buscar_tentativa_aberta()
        if tentativa is None:
            return
        execucao = tentativa.execucao
        if execucao.status != "em_andamento":
            return

        sem_sinal = monitor.segundos_sem_sinal(agora)
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
            self._fechar_tentativa_falha(tentativa, execucao, agora)
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
            self._interrupcao_pendente = False
            return

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
            self._fechar_tentativa_falha(tentativa, execucao, agora)
            execucao.status = "cancelada"
            execucao.encerrada_em = agora
            self._interrupcao_pendente = False

    def comandos_pendentes(self) -> list[Comando]:
        if self._interrupcao_pendente:
            return [Comando()]
        return []

    def _aplicar_hc_resultado(
        self,
        repo: Repositorio,
        tentativa: Tentativa,
        execucao: ExecucaoLogica,
        mensagem: HcResultado,
        agora: datetime,
    ) -> None:
        if tentativa.status != "health-check":
            return

        tipo_esperado = execucao.labirinto.tipo
        dip_ok = mensagem.tipo_dip == tipo_esperado
        if mensagem.aprovado and dip_ok:
            tentativa.status = "running"
            tentativa.tipo_dip = mensagem.tipo_dip
            return

        motivo = "health_check_failed"
        self._registrar_falha(
            repo,
            tentativa,
            motivo=motivo,
            origem="automatica",
            x=0,
            y=0,
            agora=agora,
        )
        self._fechar_tentativa_falha(tentativa, execucao, agora)

    def _aplicar_falha_telemetria(
        self,
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
        self._fechar_tentativa_falha(tentativa, execucao, agora)
        if mensagem.origem == "boot":
            self._interrupcao_pendente = False

    def _aplicar_sucesso(
        self,
        tentativa: Tentativa,
        execucao: ExecucaoLogica,
        mensagem: Sucesso,
        agora: datetime,
    ) -> None:
        if tentativa.status != "running":
            return
        tentativa.status = "success"
        tentativa.encerrada_em = agora
        execucao.status = "concluida"
        execucao.encerrada_em = agora
        execucao.tempo_total_s = Decimal(str(round((agora - execucao.iniciada_em).total_seconds(), 3)))
        self._interrupcao_pendente = False

    def _confirmar_interrupcao(self, tentativa: Tentativa | None) -> None:
        if tentativa is not None and tentativa.status in ABERTA:
            return
        self._interrupcao_pendente = False

    def _fechar_tentativa_falha(
        self, tentativa: Tentativa, execucao: ExecucaoLogica, agora: datetime
    ) -> None:
        tentativa.status = "failed"
        tentativa.encerrada_em = agora
        if tentativa.attempt_index >= self.limite_tentativas:
            execucao.status = "cancelada"
            execucao.encerrada_em = agora

    def _cancelar_execucao(self, sessao: Session, execucao: ExecucaoLogica, agora: datetime) -> None:
        repo = Repositorio(sessao)
        aberta = repo.buscar_tentativa_aberta_da_execucao(execucao.execucao_id)
        if aberta is not None:
            aberta.status = "failed"
            aberta.encerrada_em = agora
        execucao.status = "cancelada"
        execucao.encerrada_em = agora

    def _abrir_tentativa(
        self,
        repo: Repositorio,
        execucao: ExecucaoLogica,
        *,
        attempt_index: int,
        tipo_inicio: str,
        agora: datetime,
    ) -> Tentativa:
        tentativa = Tentativa(
            tentativa_id=uuid.uuid4(),
            execucao_id=execucao.execucao_id,
            attempt_index=attempt_index,
            status="health-check",
            tipo_inicio=tipo_inicio,
            iniciada_em=agora,
        )
        repo.salvar(tentativa)
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
