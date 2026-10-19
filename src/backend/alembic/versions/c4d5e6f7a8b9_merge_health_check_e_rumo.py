"""merge health-check and telemetry heading revisions

Revision ID: c4d5e6f7a8b9
Revises: 3f1c9a2b7d40, b7c8d9e0f1a2
Create Date: 2026-10-15 00:00:00.000000

"""
from collections.abc import Sequence

# revision identifiers, used by Alembic.
revision: str = "c4d5e6f7a8b9"
down_revision: tuple[str, str] | None = ("3f1c9a2b7d40", "b7c8d9e0f1a2")
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    pass


def downgrade() -> None:
    pass
