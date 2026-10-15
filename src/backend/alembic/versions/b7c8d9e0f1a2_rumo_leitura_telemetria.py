"""rumo em leitura_telemetria

Revision ID: b7c8d9e0f1a2
Revises: a1b2c3d4e5f6
Create Date: 2026-10-15 00:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "b7c8d9e0f1a2"
down_revision: str | None = "a1b2c3d4e5f6"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column("leitura_telemetria", sa.Column("rumo", sa.String(), nullable=True))
    op.create_check_constraint(
        "ck_leitura_telemetria_rumo",
        "leitura_telemetria",
        "rumo IS NULL OR rumo IN ('N', 'S', 'L', 'O')",
    )


def downgrade() -> None:
    op.drop_constraint("ck_leitura_telemetria_rumo", "leitura_telemetria", type_="check")
    op.drop_column("leitura_telemetria", "rumo")
