import uuid
from datetime import datetime
from decimal import Decimal

from app.db import Base
from sqlalchemy import CheckConstraint, DateTime, ForeignKey, Integer, Numeric, String
from sqlalchemy.dialects.postgresql import UUID
from sqlalchemy.orm import Mapped, mapped_column, relationship


class ExecucaoLogica(Base):
    __tablename__ = "execucao_logica"
    __table_args__ = (
        CheckConstraint(
            "status IN ('em_andamento', 'concluida', 'cancelada')",
            name="ck_execucao_logica_status",
        ),
        CheckConstraint(
            "tentativas_usadas BETWEEN 0 AND 3",
            name="ck_execucao_logica_tentativas_usadas",
        ),
    )

    execucao_id: Mapped[uuid.UUID] = mapped_column(UUID(as_uuid=True), primary_key=True)
    labirinto_id: Mapped[int] = mapped_column(ForeignKey("labirinto.labirinto_id"), nullable=False)
    status: Mapped[str] = mapped_column(String, nullable=False)
    tentativas_usadas: Mapped[int] = mapped_column(Integer, nullable=False, default=0)
    iniciada_em: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)
    encerrada_em: Mapped[datetime | None] = mapped_column(DateTime(timezone=True))
    tempo_total_s: Mapped[Decimal | None] = mapped_column(Numeric)

    labirinto: Mapped["Labirinto"] = relationship(back_populates="execucoes")
    tentativas: Mapped[list["Tentativa"]] = relationship(back_populates="execucao")
    paredes: Mapped[list["ParedeCelula"]] = relationship(back_populates="execucao")
    passos_trajeto: Mapped[list["PassoTrajeto"]] = relationship(back_populates="execucao")
    rejeicoes: Mapped[list["TentativaRejeitada"]] = relationship(back_populates="execucao")
