import uuid
from datetime import datetime

from app.db import Base
from sqlalchemy import BigInteger, CheckConstraint, DateTime, ForeignKey
from sqlalchemy.orm import Mapped, mapped_column, relationship


class TentativaRejeitada(Base):
    __tablename__ = "tentativa_rejeitada"
    __table_args__ = (
        CheckConstraint(
            "motivo IN ('tentativa_aberta', 'limite_3', 'execucao_cancelada')",
            name="ck_tentativa_rejeitada_motivo",
        ),
    )

    rejeicao_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    execucao_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("execucao_logica.execucao_id"), nullable=False
    )
    motivo: Mapped[str] = mapped_column(nullable=False)
    ocorrida_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    execucao: Mapped["ExecucaoLogica"] = relationship(back_populates="rejeicoes")
