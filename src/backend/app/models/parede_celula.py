import uuid
from datetime import datetime

from app.db import Base
from sqlalchemy import BigInteger, Boolean, DateTime, ForeignKey, Integer, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship


class ParedeCelula(Base):
    __tablename__ = "parede_celula"
    __table_args__ = (UniqueConstraint("execucao_id", "x", "y", name="uq_parede_execucao_celula"),)

    parede_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    execucao_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("execucao_logica.execucao_id"), nullable=False
    )
    x: Mapped[int] = mapped_column(Integer, nullable=False)
    y: Mapped[int] = mapped_column(Integer, nullable=False)
    norte: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    sul: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    leste: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    oeste: Mapped[bool] = mapped_column(Boolean, nullable=False, default=False)
    detectada_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    execucao: Mapped["ExecucaoLogica"] = relationship(back_populates="paredes")
