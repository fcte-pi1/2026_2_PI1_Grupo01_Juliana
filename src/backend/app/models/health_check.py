from __future__ import annotations

import uuid
from datetime import datetime

from app.db import Base
from sqlalchemy import BigInteger, Boolean, CheckConstraint, DateTime, String
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import Mapped, mapped_column, relationship


class HealthCheck(Base):
    """Health-check do robô, pré-requisito para abrir qualquer tentativa.

    Fica em andamento (`aprovado` e `concluido_em` nulos) do primeiro
    `hc_item` até o `hc_resultado` do mesmo boot.
    """

    __tablename__ = "health_check"
    __table_args__ = (
        CheckConstraint(
            "tipo_dip IN ('4x4', '8x4', '12x4', 'invalido')",
            name="ck_health_check_tipo_dip",
        ),
    )

    hc_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True)
    boot: Mapped[int | None] = mapped_column(BigInteger)
    aprovado: Mapped[bool | None] = mapped_column(Boolean)
    tipo_dip: Mapped[str | None] = mapped_column(String)
    iniciado_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    concluido_em: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))

    itens: Mapped[list[HealthCheckItem]] = relationship(
        back_populates="health_check", order_by="HealthCheckItem.verificado_em"
    )
    tentativas: Mapped[list[Tentativa]] = relationship(back_populates="health_check")
