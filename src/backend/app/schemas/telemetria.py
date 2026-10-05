from datetime import datetime
from typing import Literal

from pydantic import BaseModel, Field


class MensagemTelemetria(BaseModel):
    """Contrato ARQ-01 — esqueleto; campos finais alinhados ao diagrama de componentes."""

    seq: int = Field(ge=0)
    status: Literal["health-check", "running", "success", "failed"]
    x: int = Field(ge=0)
    y: int = Field(ge=0)
    bateria: float = Field(ge=0)
    velocidade: float | None = None
    enviado_em: datetime
    tipo_labirinto_descoberto: Literal["4x4", "8x4", "12x4", "indeterminado"] | None = None
    tipo_inicio: Literal["nova", "retomada"] | None = None


class RespostaTelemetria(BaseModel):
    aceita: bool
    interrupcao_pendente: bool = False
