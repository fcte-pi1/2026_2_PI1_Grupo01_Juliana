from app.db import Base
from sqlalchemy import Integer, String, UniqueConstraint
from sqlalchemy.orm import Mapped, mapped_column, relationship


class Labirinto(Base):
    __tablename__ = "labirinto"
    __table_args__ = (UniqueConstraint("tipo", name="uq_labirinto_tipo"),)

    labirinto_id: Mapped[int] = mapped_column(Integer, primary_key=True)
    tipo: Mapped[str] = mapped_column(String(5), nullable=False)
    largura: Mapped[int] = mapped_column(Integer, nullable=False)
    altura: Mapped[int] = mapped_column(Integer, nullable=False)

    execucoes: Mapped[list["ExecucaoLogica"]] = relationship(back_populates="labirinto")
