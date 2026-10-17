"""consumo_bateria_pct

Revision ID: a1b2c3d4e5f6
Revises: 88a55c282858
Create Date: 2026-10-13 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "a1b2c3d4e5f6"
down_revision: str | None = "88a55c282858"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column("tentativa", sa.Column("consumo_bateria_pct", sa.Numeric(), nullable=True))


def downgrade() -> None:
    op.drop_column("tentativa", "consumo_bateria_pct")
