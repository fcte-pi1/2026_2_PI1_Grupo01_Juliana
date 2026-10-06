from uuid import UUID

from app.routers.deps import nao_implementado
from fastapi import APIRouter

router = APIRouter(prefix="/execucoes", tags=["stream"])


@router.get("/{execucao_id}/stream")
async def stream_execucao(execucao_id: UUID) -> None:
    nao_implementado()
