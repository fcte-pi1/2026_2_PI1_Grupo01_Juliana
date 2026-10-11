"""Persistência de leituras e passos de trajeto (BACK-06)."""

from datetime import datetime
from uuid import UUID

from app.models import LeituraTelemetria, ParedeCelula, PassoTrajeto
from sqlalchemy import func, select
from sqlalchemy.dialects.postgresql import insert
from sqlalchemy.orm import Session

# máscara de paredes do `passo` (contrato v1)
NORTE, SUL, LESTE, OESTE = 1, 2, 4, 8


class TelemetriaRepository:
    def __init__(self, session: Session) -> None:
        self._session = session

    def contar_leituras(self) -> int:
        stmt = select(func.count()).select_from(LeituraTelemetria)
        return int(self._session.scalar(stmt) or 0)

    def proxima_ordem_leitura(self, tentativa_id: UUID) -> int:
        stmt = select(func.coalesce(func.max(LeituraTelemetria.ordem), 0)).where(
            LeituraTelemetria.tentativa_id == tentativa_id
        )
        return int(self._session.scalar(stmt)) + 1

    def ultimo_passo(self, execucao_id: UUID) -> PassoTrajeto | None:
        stmt = (
            select(PassoTrajeto)
            .where(PassoTrajeto.execucao_id == execucao_id)
            .order_by(PassoTrajeto.seq.desc())
            .limit(1)
        )
        return self._session.scalar(stmt)

    def listar_trajeto(self, execucao_id: UUID) -> list[PassoTrajeto]:
        stmt = (
            select(PassoTrajeto)
            .where(PassoTrajeto.execucao_id == execucao_id)
            .order_by(PassoTrajeto.seq)
        )
        return list(self._session.scalars(stmt))

    def gravar_paredes(
        self, execucao_id: UUID, x: int, y: int, mascara: int, detectada_em: datetime
    ) -> None:
        """Upsert da célula: a última leitura vence (D7)."""
        paredes = {
            "norte": bool(mascara & NORTE),
            "sul": bool(mascara & SUL),
            "leste": bool(mascara & LESTE),
            "oeste": bool(mascara & OESTE),
            "detectada_em": detectada_em,
        }
        stmt = insert(ParedeCelula).values(execucao_id=execucao_id, x=x, y=y, **paredes)
        stmt = stmt.on_conflict_do_update(constraint="uq_parede_execucao_celula", set_=paredes)
        self._session.execute(stmt)
