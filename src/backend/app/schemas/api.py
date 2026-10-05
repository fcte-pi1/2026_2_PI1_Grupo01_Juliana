from datetime import datetime
from decimal import Decimal
from typing import Literal
from uuid import UUID

from pydantic import BaseModel, Field

TipoLabirinto = Literal["4x4", "8x4", "12x4"]
StatusExecucao = Literal["em_andamento", "concluida", "cancelada"]
StatusTentativa = Literal["health-check", "running", "success", "failed"]
MotivoEncerramento = Literal["collision", "stuck", "out_of_track"]


class BackHealthResponse(BaseModel):
    status: Literal["ok", "degraded"]
    banco_acessivel: bool


class NovaExecucaoRequest(BaseModel):
    tipo_labirinto: TipoLabirinto


class EncerrarTentativaRequest(BaseModel):
    motivo: MotivoEncerramento
    observacao: str | None = Field(default=None, max_length=100)


class CorrigirTipoLabirintoRequest(BaseModel):
    tipo_labirinto: TipoLabirinto


class TentativaResumo(BaseModel):
    tentativa_id: UUID
    attempt_index: int
    status: StatusTentativa
    tipo_inicio: Literal["nova", "retomada"]
    iniciada_em: datetime
    encerrada_em: datetime | None = None


class ExecucaoResumo(BaseModel):
    execucao_id: UUID
    tipo_labirinto: TipoLabirinto
    status: StatusExecucao
    tentativas_usadas: int
    iniciada_em: datetime
    encerrada_em: datetime | None = None


class ExecucaoDetalhe(ExecucaoResumo):
    tentativas: list[TentativaResumo] = Field(default_factory=list)
    tempo_total_s: Decimal | None = None
    variacao_melhor_tempo_pct: Decimal | None = None


class ExecucaoCriadaResponse(BaseModel):
    execucao_id: UUID
    tentativa_id: UUID
    attempt_index: int = 1
