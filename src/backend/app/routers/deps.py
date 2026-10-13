from fastapi import HTTPException, status


def nao_implementado() -> None:
    raise HTTPException(
        status_code=status.HTTP_501_NOT_IMPLEMENTED,
        detail="Endpoint registrado no contrato ARQ-02; implementação pendente.",
    )
