"""health_check_tres_tof

Troca os dois ToF frontais (tof_frontal_esq, tof_frontal_dir) pelo frontal único
(tof_frontal) no CHECK de health_check_item: o robô passou a ter 3 ToF (docs/4.1).

Revision ID: 3f1c9a2b7d40
Revises: 88a55c282858
Create Date: 2026-10-08 12:00:00.000000

"""
from collections.abc import Sequence

from alembic import op

# revision identifiers, used by Alembic.
revision: str = '3f1c9a2b7d40'
down_revision: str | None = '88a55c282858'
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None

CHECK = 'ck_health_check_item_componente'

COMPONENTES_8 = (
    "'bateria', 'tof_frontal', 'tof_esquerdo', 'tof_direito', "
    "'motor_esquerdo', 'motor_direito', 'encoder_esquerdo', 'encoder_direito'"
)

COMPONENTES_9 = (
    "'bateria', 'tof_frontal_esq', 'tof_frontal_dir', 'tof_esquerdo', 'tof_direito', "
    "'motor_esquerdo', 'motor_direito', 'encoder_esquerdo', 'encoder_direito'"
)


def upgrade() -> None:
    op.execute(
        "DELETE FROM health_check_item "
        "WHERE componente IN ('tof_frontal_esq', 'tof_frontal_dir')"
    )
    op.drop_constraint(CHECK, 'health_check_item', type_='check')
    op.create_check_constraint(CHECK, 'health_check_item', f"componente IN ({COMPONENTES_8})")


def downgrade() -> None:
    op.execute("DELETE FROM health_check_item WHERE componente = 'tof_frontal'")
    op.drop_constraint(CHECK, 'health_check_item', type_='check')
    op.create_check_constraint(CHECK, 'health_check_item', f"componente IN ({COMPONENTES_9})")
