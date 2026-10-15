"""merge heads health_check e rumo

Une as duas heads que saíram de 88a55c282858:
3f1c9a2b7d40 (ToF no health check) e b7c8d9e0f1a2 (consumo e rumo).

Revision ID: c9d0e1f2a3b4
Revises: 3f1c9a2b7d40, b7c8d9e0f1a2
Create Date: 2026-10-15 02:36:00.000000

"""

from collections.abc import Sequence

revision: str = "c9d0e1f2a3b4"
down_revision: str | Sequence[str] | None = ("3f1c9a2b7d40", "b7c8d9e0f1a2")
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
