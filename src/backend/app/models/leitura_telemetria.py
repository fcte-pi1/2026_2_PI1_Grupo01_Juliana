import uuid
from datetime import datetime
from decimal import Decimal

from app.db import Base
from sqlalchemy import (
    BigInteger,
    CheckConstraint,
    DateTime,
    ForeignKey,
    Index,
    Integer,
    Numeric,
    UniqueConstraint,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship


class LeituraTelemetria(Base):
    __tablename__ = "leitura_telemetria"
    __table_args__ = (
        CheckConstraint(
            "fase IN ('health-check', 'running')",
            name="ck_leitura_telemetria_fase",
        ),
        UniqueConstraint("tentativa_id", "ordem", name="uq_leitura_tentativa_ordem"),
        Index("ix_leitura_telemetria_tentativa_leitura", "tentativa_id", "leitura_id"),
    )

    leitura_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    tentativa_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("tentativa.tentativa_id"), nullable=False
    )
    ordem: Mapped[int] = mapped_column(Integer, nullable=False)
    fase: Mapped[str] = mapped_column(nullable=False)
    x: Mapped[int | None] = mapped_column(Integer)
    y: Mapped[int | None] = mapped_column(Integer)
    bateria: Mapped[Decimal | None] = mapped_column(Numeric)
    velocidade: Mapped[Decimal | None] = mapped_column(Numeric)
    enviado_em: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    recebido_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    tentativa: Mapped["Tentativa"] = relationship(back_populates="leituras")
