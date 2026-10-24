from datetime import datetime
from typing import Literal
from uuid import UUID

from pydantic import BaseModel, Field

Labirinto = Literal["4x4", "8x4", "12x4"]
TipoLabirinto = Labirinto
StatusExecucao = Literal["em_andamento", "concluida", "cancelada"]
StatusTentativa = Literal["running", "success", "failed"]
TipoInicio = Literal["nova", "retomada"]
TipoDip = Literal["4x4", "8x4", "12x4", "invalido"]
Componente = Literal[
    "bateria",
    "tof_frontal",
    "tof_esquerdo",
    "tof_direito",
    "motor_esquerdo",
    "motor_direito",
    "encoder_esquerdo",
    "encoder_direito",
]
MotivoFalha = Literal[
    "collision",
    "stuck",
    "out_of_track",
    "time_exceeded",
    "falha_componente",
    "low_battery",
    "health_check_failed",
    "link_lost",
    "encerrado_operador",
]
MotivoEncerramento = Literal["collision", "stuck", "out_of_track"]
MotivoRecusa = Literal[
    "tentativa_aberta",
    "limite_3",
    "execucao_cancelada",
    "health_check_ausente",
    "health_check_reprovado",
    "health_check_expirado",
    "dip_divergente",
]
OrigemFalha = Literal["automatica", "encerrado_operador"]
Rumo = Literal["N", "S", "L", "O"]
EixoLongo = Literal["x", "y"]


class BackHealthResponse(BaseModel):
    status: Literal["ok", "degraded"]
    banco_acessivel: bool


class NovaExecucaoRequest(BaseModel):
    tipo_labirinto: Labirinto


class EncerrarTentativaRequest(BaseModel):
    motivo: MotivoEncerramento
    observacao: str | None = Field(default=None, max_length=100)


class ItemHealthCheck(BaseModel):
    componente: Componente
    aprovado: bool
    valor_lido: float | None = None


class UltimoHealthCheck(BaseModel):
    aprovado: bool | None = None
    tipo_dip: TipoDip | None = None
    iniciado_em: datetime
    concluido_em: datetime | None = None
    valido_ate: datetime | None = None
    itens: list[ItemHealthCheck] = Field(default_factory=list)


class EventoHealthCheckAtual(UltimoHealthCheck):
    """Retrato do health-check na stream própria, antes de existir execução."""

    tipo: Literal["health_check"] = "health_check"


class SolicitacaoHealthCheck(BaseModel):
    solicitado_em: datetime


class Falha(BaseModel):
    motivo: MotivoFalha
    origem: OrigemFalha
    celula_x: int = Field(ge=0, le=11)
    celula_y: int = Field(ge=0, le=11)
    componente: Componente | None = None
    observacao: str | None = Field(default=None, max_length=100)
    momento_falha: datetime


class Tentativa(BaseModel):
    tentativa_id: UUID
    attempt_index: int = Field(ge=1, le=3)
    status: StatusTentativa
    tipo_inicio: TipoInicio
    tipo_dip: TipoDip | None = None
    iniciada_em: datetime
    encerrada_em: datetime | None = None
    tempo_s: float | None = None
    velocidade_media: float | None = None
    bateria_inicial: float | None = None
    bateria_final: float | None = None
    consumo_bateria: float | None = None
    consumo_bateria_pct: float | None = None
    health_check: list[ItemHealthCheck] = Field(default_factory=list)
    falha: Falha | None = None


class PassoTrajeto(BaseModel):
    passo_id: int
    seq: int = Field(ge=1)
    x: int = Field(ge=0, le=11)
    y: int = Field(ge=0, le=11)
    paredes_mask: int = Field(ge=0, le=15)
    retomada: bool
    entrou_em: datetime | None = None


class LeituraTelemetria(BaseModel):
    ordem: int = Field(ge=1)
    x: int = Field(ge=0, le=11)
    y: int = Field(ge=0, le=11)
    bateria: float
    velocidade: float | None = None
    rumo: Rumo | None = None
    enviado_em: datetime


class ResumoExecucao(BaseModel):
    execucao_id: UUID
    tipo_labirinto: Labirinto
    status: StatusExecucao
    tentativas_usadas: int = Field(ge=0, le=3)
    iniciada_em: datetime
    encerrada_em: datetime | None = None
    tempo_total_s: float | None = None
    velocidade_media: float | None = None
    consumo_bateria: float | None = None


class ExecucaoDetalhe(BaseModel):
    execucao_id: UUID
    tipo_labirinto: Labirinto
    status: StatusExecucao
    tentativas_usadas: int = Field(ge=0, le=3)
    iniciada_em: datetime
    encerrada_em: datetime | None = None
    tempo_total_s: float | None = None
    variacao_melhor_tempo_pct: float | None = None
    eixo_longo: EixoLongo | None = None
    ultima_mensagem_em: datetime | None = None
    tentativas: list[Tentativa] = Field(default_factory=list)
    trajeto: list[PassoTrajeto] = Field(default_factory=list)
    leituras: list[LeituraTelemetria] = Field(default_factory=list)


class RetornoNovaExecucao(BaseModel):
    execucao_id: UUID
    tentativa_id: UUID
    attempt_index: int = Field(ge=1, le=3)


class Recusa(BaseModel):
    motivo: MotivoRecusa
    ocorrida_em: datetime


# Alias usado antes do alinhamento ARQ-02
ExecucaoCriadaResponse = RetornoNovaExecucao
ExecucaoResumo = ResumoExecucao


class EventoTelemetria(BaseModel):
    tipo: Literal["telemetria"] = "telemetria"
    ordem: int = Field(ge=1)
    x: int = Field(ge=0, le=11)
    y: int = Field(ge=0, le=11)
    bateria: int
    bateria_pct: float | None = None
    velocidade: int | None = None
    rumo: Rumo | None = None
    eixo_longo: EixoLongo | None = None
    enviado_em: datetime


class EventoPasso(BaseModel):
    tipo: Literal["passo"] = "passo"
    passo_id: int
    seq: int = Field(ge=1)
    x: int = Field(ge=0, le=11)
    y: int = Field(ge=0, le=11)
    paredes_mask: int = Field(ge=0, le=15)
    retomada: bool
    # coluna obrigatória no banco: o evento nunca sai com entrou_em nulo
    entrou_em: datetime


class EventoStatus(BaseModel):
    tipo: Literal["status"] = "status"
    tentativa_id: UUID
    attempt_index: int = Field(ge=1, le=3)
    status_tentativa: StatusTentativa
    status_execucao: StatusExecucao
    tipo_dip: TipoDip | None = None


class EventoHealthCheck(BaseModel):
    tipo: Literal["health_check"] = "health_check"
    tentativa_id: UUID
    componente: Componente
    aprovado: bool
    valor_lido: float | None = None


class EventoFalha(BaseModel):
    tipo: Literal["falha"] = "falha"
    tentativa_id: UUID
    motivo: MotivoFalha
    origem: OrigemFalha
    celula_x: int = Field(ge=0, le=11)
    celula_y: int = Field(ge=0, le=11)
    componente: Componente | None = None
    observacao: str | None = Field(default=None, max_length=100)
    momento_falha: datetime
    status_execucao: StatusExecucao


class EventoSucesso(BaseModel):
    tipo: Literal["sucesso"] = "sucesso"
    tentativa_id: UUID
    x: int = Field(ge=0, le=11)
    y: int = Field(ge=0, le=11)
    status_execucao: StatusExecucao
