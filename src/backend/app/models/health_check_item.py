import uuid
from datetime import datetime
from decimal import Decimal

from app.db import Base
from sqlalchemy import BigInteger, Boolean, CheckConstraint, DateTime, ForeignKey, Numeric
from sqlalchemy.orm import Mapped, mapped_column, relationship


class HealthCheckItem(Base):
    __tablename__ = "health_check_item"
    __table_args__ = (
        CheckConstraint(
            "componente IN ("
            "'bateria', 'tof_frontal', 'tof_esquerdo', 'tof_direito', "
            "'motor_esquerdo', 'motor_direito', "
            "'encoder_esquerdo', 'encoder_direito'"
            ")",
            name="ck_health_check_item_componente",
        ),
    )

    item_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    hc_id: Mapped[uuid.UUID] = mapped_column(ForeignKey("health_check.hc_id"), nullable=False)
    componente: Mapped[str] = mapped_column(nullable=False)
    aprovado: Mapped[bool] = mapped_column(Boolean, nullable=False)
    valor_lido: Mapped[Decimal | None] = mapped_column(Numeric)
    verificado_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    health_check: Mapped["HealthCheck"] = relationship(back_populates="itens")
