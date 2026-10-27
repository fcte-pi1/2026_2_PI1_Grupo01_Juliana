"""Fábricas dos eventos do stream SSE (BACK-07).

Os seis eventos têm payloads diferentes, mas a mesma cerimônia: montar o modelo
Pydantic, serializar e decidir se o frame leva linha `id:`. A cerimônia fica em
`FabricaEvento.criar` e o que varia desce para o `montar` de cada subclasse.

O payload é montado antes do COMMIT, porque o `passo_id` já existe depois do
flush, mas só é publicado depois dele (ver `IngestaoTelemetria`).
"""

import logging
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass
from datetime import datetime
from typing import Any, ClassVar

from app import models
from app.schemas import api
from app.services.calculo_metricas import percentual_carga_mv
from app.services.publicador_sse import EventoSSE
from contrato.telemetria import Tel
from pydantic import BaseModel

logger = logging.getLogger("app.sse")


@dataclass(frozen=True, slots=True)
class OrigemTelemetria:
    """A `tel` recebida, com a ordem e o horário calculados pela ingestão.

    Vem da mensagem, e não da linha gravada, porque `rumo` e `eixo_longo` fazem
    parte do contrato serial e não são persistidos.
    """

    tel: Tel
    ordem: int
    enviado_em: datetime


@dataclass(frozen=True, slots=True)
class OrigemStatus:
    tentativa: models.Tentativa
    status_execucao: api.StatusExecucao


@dataclass(frozen=True, slots=True)
class OrigemFalha:
    falha: models.Falha
    status_execucao: api.StatusExecucao


@dataclass(frozen=True, slots=True)
class OrigemSucesso:
    tentativa: models.Tentativa
    x: int
    y: int
    status_execucao: api.StatusExecucao


@dataclass(frozen=True, slots=True)
class OrigemHealthCheck:
    """Item gravado e a tentativa a que o evento se refere.

    A linha não guarda mais a tentativa: o vínculo fica em tentativa.hc_id,
    e só existe depois da largada.
    """

    item: models.HealthCheckItem
    tentativa_id: uuid.UUID


class FabricaEvento(ABC):
    """Fábrica de um tipo de evento SSE."""

    tipo: ClassVar[str]

    @abstractmethod
    def montar(self, origem: Any) -> BaseModel:
        """Monta o payload. É o factory method que cada evento implementa."""

    def identificador(self, origem: Any) -> int | None:
        """Valor da linha `id:`. Só o passo tem; o default serve aos outros cinco."""
        return None

    def encerra_execucao(self, origem: Any) -> bool:
        """Se a execução acaba neste evento. Só falha e sucesso podem encerrá-la."""
        return False

    def criar(self, origem: Any) -> EventoSSE:
        modelo = self.montar(origem)
        return EventoSSE(
            tipo=self.tipo,
            dados=modelo.model_dump_json(),
            id=self.identificador(origem),
            encerra=self.encerra_execucao(origem),
        )


class FabricaPasso(FabricaEvento):
    """Serve à ingestão e ao reenvio, então o passo ao vivo e o reenviado são iguais."""

    tipo = "passo"

    def identificador(self, origem: models.PassoTrajeto) -> int | None:
        return origem.passo_id

    def montar(self, origem: models.PassoTrajeto) -> api.EventoPasso:
        return api.EventoPasso(
            passo_id=origem.passo_id,
            seq=origem.seq,
            x=origem.x,
            y=origem.y,
            # coluna nulável; o contrato exige a máscara entre 0 e 15
            paredes_mask=origem.paredes_mask or 0,
            retomada=origem.retomada,
            entrou_em=origem.entrou_em,
        )


class FabricaTelemetria(FabricaEvento):
    tipo = "telemetria"

    def montar(self, origem: OrigemTelemetria) -> api.EventoTelemetria:
        tel = origem.tel
        return api.EventoTelemetria(
            ordem=origem.ordem,
            x=tel.x,
            y=tel.y,
            bateria=tel.bat_mv,
            bateria_pct=percentual_carga_mv(tel.bat_mv),
            velocidade=tel.vel_mm_s,
            rumo=tel.rumo,
            eixo_longo=tel.eixo_longo,
            enviado_em=origem.enviado_em,
        )


class FabricaHealthCheck(FabricaEvento):
    tipo = "health_check"

    def montar(self, origem: OrigemHealthCheck) -> api.EventoHealthCheck:
        item = origem.item
        return api.EventoHealthCheck(
            tentativa_id=origem.tentativa_id,
            componente=item.componente,
            aprovado=item.aprovado,
            valor_lido=item.valor_lido,
        )


class FabricaStatus(FabricaEvento):
    """Preparada para o BACK-03, que monta a origem depois das transições."""

    tipo = "status"

    def montar(self, origem: OrigemStatus) -> api.EventoStatus:
        tentativa = origem.tentativa
        return api.EventoStatus(
            tentativa_id=tentativa.tentativa_id,
            attempt_index=tentativa.attempt_index,
            status_tentativa=tentativa.status,
            status_execucao=origem.status_execucao,
            tipo_dip=tentativa.tipo_dip,
        )


class FabricaFalha(FabricaEvento):
    """Preparada para o BACK-03, que cria a linha de FALHA antes do COMMIT."""

    tipo = "falha"

    def encerra_execucao(self, origem: OrigemFalha) -> bool:
        return origem.status_execucao != "em_andamento"

    def montar(self, origem: OrigemFalha) -> api.EventoFalha:
        falha = origem.falha
        return api.EventoFalha(
            tentativa_id=falha.tentativa_id,
            motivo=falha.motivo,
            origem=falha.origem,
            celula_x=falha.celula_x,
            celula_y=falha.celula_y,
            componente=falha.componente,
            observacao=falha.observacao,
            momento_falha=falha.momento_falha,
            status_execucao=origem.status_execucao,
        )


class FabricaSucesso(FabricaEvento):
    """Preparada para o BACK-03, que fecha a tentativa antes do COMMIT."""

    tipo = "sucesso"

    def encerra_execucao(self, origem: OrigemSucesso) -> bool:
        return origem.status_execucao != "em_andamento"

    def montar(self, origem: OrigemSucesso) -> api.EventoSucesso:
        return api.EventoSucesso(
            tentativa_id=origem.tentativa.tentativa_id,
            x=origem.x,
            y=origem.y,
            status_execucao=origem.status_execucao,
        )


class RegistroDeFabricas:
    """Resolve a fábrica pelo tipo do objeto de origem."""

    def __init__(self) -> None:
        self._fabricas: dict[type, FabricaEvento] = {}

    def registrar(self, tipo_origem: type, fabrica: FabricaEvento) -> None:
        self._fabricas[tipo_origem] = fabrica

    def criar(self, origem: object) -> EventoSSE:
        fabrica = self._fabricas.get(type(origem))
        if fabrica is None:
            raise TypeError(f"sem fábrica de evento SSE para {type(origem).__name__}")
        return fabrica.criar(origem)


FABRICAS = RegistroDeFabricas()
FABRICAS.registrar(models.PassoTrajeto, FabricaPasso())
FABRICAS.registrar(OrigemTelemetria, FabricaTelemetria())
FABRICAS.registrar(OrigemHealthCheck, FabricaHealthCheck())
FABRICAS.registrar(OrigemStatus, FabricaStatus())
FABRICAS.registrar(OrigemFalha, FabricaFalha())
FABRICAS.registrar(OrigemSucesso, FabricaSucesso())


def criar_eventos(origem: object) -> list[EventoSSE]:
    """Evento da origem, ou lista vazia se a montagem falhar.

    A falha é registrada e engolida de propósito: montar o payload acontece dentro
    da transação, e um erro aqui não pode desfazer a gravação de uma mensagem
    válida. O custo é a atualização ao vivo, que o reenvio recupera.
    """
    try:
        return [FABRICAS.criar(origem)]
    except Exception:
        logger.exception("evento SSE não montado para %s", type(origem).__name__)
        return []
