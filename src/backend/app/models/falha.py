import uuid
from datetime import datetime

from app.db import Base
from sqlalchemy import BigInteger, CheckConstraint, DateTime, ForeignKey, Integer, String
from sqlalchemy.orm import Mapped, mapped_column, relationship


class Falha(Base):
    __tablename__ = "falha"
    __table_args__ = (
        CheckConstraint(
            "motivo IN ("
            "'collision', 'stuck', 'out_of_track', 'time_exceeded', "
            "'falha_componente', 'low_battery', 'health_check_failed', "
            "'link_lost', 'encerrado_operador'"
            ")",
            name="ck_falha_motivo",
        ),
        CheckConstraint(
            "origem IN ('automatica', 'encerrado_operador')",
            name="ck_falha_origem",
        ),
    )

    falha_id: Mapped[int] = mapped_column(BigInteger, primary_key=True, autoincrement=True)
    tentativa_id: Mapped[uuid.UUID] = mapped_column(
        ForeignKey("tentativa.tentativa_id"), unique=True, nullable=False
    )
    motivo: Mapped[str] = mapped_column(String, nullable=False)
    origem: Mapped[str] = mapped_column(String, nullable=False)
    celula_x: Mapped[int | None] = mapped_column(Integer)
    celula_y: Mapped[int | None] = mapped_column(Integer)
    componente: Mapped[str | None] = mapped_column(String)
    observacao: Mapped[str | None] = mapped_column(String(100))
    momento_falha: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)

    tentativa: Mapped["Tentativa"] = relationship(back_populates="falha")
