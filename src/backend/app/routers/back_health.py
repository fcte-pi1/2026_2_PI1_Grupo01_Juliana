from app.db import banco_acessivel
from app.schemas.api import BackHealthResponse
from fastapi import APIRouter

router = APIRouter(tags=["back-health"])


@router.get("/back-health", response_model=BackHealthResponse)
def back_health() -> BackHealthResponse:
    db_ok = banco_acessivel()
    return BackHealthResponse(
        status="ok" if db_ok else "degraded",
        banco_acessivel=db_ok,
    )
