import uuid
from datetime import datetime

from app.db import Base
from sqlalchemy import BigInteger, Boolean, DateTime, ForeignKey, Index, Integer, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship


class PassoTrajeto(Base):
    __tablename__ = "passo_trajeto"
    __table_args__ = (
        UniqueConstraint("execucao_id", "seq", name="uq_passo_execucao_seq"),
        Index("ix_passo_trajeto_execucao_passo", "execucao_id", "passo_id"),
    )

    passo_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    tentativa_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("tentativa.tentativa_id"), nullable=False
    )
    execucao_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("execucao_logica.execucao_id"), nullable=False
    )
    seq: Mapped[int] = mapped_column(Integer, nullable=False)
    x: Mapped[int] = mapped_column(Integer, nullable=False)
    y: Mapped[int] = mapped_column(Integer, nullable=False)
    paredes_mask: Mapped[int | None] = mapped_column(Integer)
    retomada: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    entrou_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    tentativa: Mapped["Tentativa"] = relationship(back_populates="passos")
    execucao: Mapped["ExecucaoLogica"] = relationship(back_populates="passos_trajeto")
