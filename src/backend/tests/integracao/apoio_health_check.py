"""Health-check concluído no banco, pré-requisito de Nova execução e Retomar."""

import uuid
from datetime import UTC, datetime

from app.models import HealthCheck
from sqlalchemy.orm import Session


def gravar_health_check(
    session: Session,
    tipo: str = "4x4",
    *,
    aprovado: bool = True,
    concluido_em: datetime | None = None,
) -> HealthCheck:
    concluido_em = concluido_em or datetime.now(UTC)
    health_check = HealthCheck(
        hc_id=uuid.uuid4(),
        boot=1,
        aprovado=aprovado,
        tipo_dip=tipo,
        iniciado_em=concluido_em,
        concluido_em=concluido_em,
    )
    session.add(health_check)
    session.flush()
    return health_check
