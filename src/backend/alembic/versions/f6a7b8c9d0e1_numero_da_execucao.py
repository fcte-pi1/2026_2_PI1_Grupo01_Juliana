"""numero_da_execucao

Cada execução ganha um número sequencial crescente (#0001, #0002...), usado na tela no lugar do UUID.
As execuções que já existem são numeradas pela ordem de início. Uma sequência do Postgres dá o próximo
número a cada execução nova, sem corrida entre duas aberturas.

Revision ID: f6a7b8c9d0e1
Revises: e5f6a7b8c9d0
Create Date: 2026-11-21 14:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "f6a7b8c9d0e1"
down_revision: str | None = "e5f6a7b8c9d0"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None


def upgrade() -> None:
    op.add_column("execucao_logica", sa.Column("numero", sa.Integer(), nullable=True))
    op.execute(
        """
        UPDATE execucao_logica AS e
        SET numero = n.posicao
        FROM (
            SELECT execucao_id,
                   row_number() OVER (ORDER BY iniciada_em, execucao_id) AS posicao
            FROM execucao_logica
        ) AS n
        WHERE e.execucao_id = n.execucao_id
        """
    )
    op.execute("CREATE SEQUENCE execucao_numero_seq OWNED BY execucao_logica.numero")
    op.execute(
        "SELECT setval('execucao_numero_seq', COALESCE((SELECT MAX(numero) FROM execucao_logica), 0) + 1, false)"
    )
    op.execute(
        "ALTER TABLE execucao_logica ALTER COLUMN numero SET DEFAULT nextval('execucao_numero_seq')"
    )
    op.alter_column("execucao_logica", "numero", nullable=False)
    op.create_unique_constraint("uq_execucao_logica_numero", "execucao_logica", ["numero"])


def downgrade() -> None:
    op.drop_constraint("uq_execucao_logica_numero", "execucao_logica", type_="unique")
    op.execute("ALTER TABLE execucao_logica ALTER COLUMN numero DROP DEFAULT")
    op.execute("DROP SEQUENCE IF EXISTS execucao_numero_seq")
    op.drop_column("execucao_logica", "numero")
