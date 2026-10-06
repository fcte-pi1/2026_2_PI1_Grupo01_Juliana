from __future__ import annotations

import uuid
from datetime import datetime
from decimal import Decimal

from app.db import Base
from sqlalchemy import (
    CheckConstraint,
    DateTime,
    ForeignKey,
    Index,
    Integer,
    Numeric,
    String,
    UniqueConstraint,
    text,
)
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import Mapped, mapped_column, relationship


class Tentativa(Base):
    __tablename__ = "tentativa"
    __table_args__ = (
        CheckConstraint("attempt_index BETWEEN 1 AND 3", name="ck_tentativa_attempt_index"),
        CheckConstraint(
            "status IN ('health-check', 'running', 'success', 'failed')",
            name="ck_tentativa_status",
        ),
        CheckConstraint(
            "tipo_inicio IN ('nova', 'retomada')",
            name="ck_tentativa_tipo_inicio",
        ),
        UniqueConstraint("execucao_id", "attempt_index", name="uq_tentativa_execucao_attempt"),
        Index(
            "uq_tentativa_aberta_por_execucao",
            "execucao_id",
            unique=True,
            postgresql_where=text("status IN ('health-check', 'running')"),
        ),
    )

    tentativa_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True)
    execucao_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("execucao_logica.execucao_id"), nullable=False
    )
    attempt_index: Mapped[int] = mapped_column(Integer, nullable=False)
    status: Mapped[str] = mapped_column(String, nullable=False)
    tipo_inicio: Mapped[str] = mapped_column(String, nullable=False)
    tipo_dip: Mapped[str | None] = mapped_column(String)
    iniciada_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    encerrada_em: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    tempo_s: Mapped[Decimal | None] = mapped_column(Numeric)
    velocidade_media: Mapped[Decimal | None] = mapped_column(Numeric)
    bateria_inicial: Mapped[Decimal | None] = mapped_column(Numeric)
    bateria_final: Mapped[Decimal | None] = mapped_column(Numeric)
    consumo_bateria: Mapped[Decimal | None] = mapped_column(Numeric)

    execucao: Mapped[ExecucaoLogica] = relationship(back_populates="tentativas")
    leituras: Mapped[list[LeituraTelemetria]] = relationship(back_populates="tentativa")
    passos: Mapped[list[PassoTrajeto]] = relationship(back_populates="tentativa")
    health_check_itens: Mapped[list[HealthCheckItem]] = relationship(back_populates="tentativa")
    falha: Mapped[Falha | None] = relationship(back_populates="tentativa", uselist=False)
