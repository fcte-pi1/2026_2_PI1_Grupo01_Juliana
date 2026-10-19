"""health_check_pre_requisito

O health-check sai da tentativa e vira pré-requisito da largada: ganha a
tabela health_check, os itens passam a apontar para ela e a tentativa só
guarda qual health-check a liberou. A tentativa nasce em running, então o
estado health-check sai do CHECK e do índice de tentativa aberta.

Os itens antigos viram um health_check por tentativa, com hc_id igual ao
tentativa_id. Tentativas que ficaram presas em health-check viram failed.

Revision ID: d4e5f6a7b8c9
Revises: c4d5e6f7a8b9
Create Date: 2026-10-17 13:00:00.000000

"""

from collections.abc import Sequence

import sqlalchemy as sa
from alembic import op

revision: str = "d4e5f6a7b8c9"
down_revision: str | None = "c4d5e6f7a8b9"
branch_labels: str | Sequence[str] | None = None
depends_on: str | Sequence[str] | None = None

MOTIVOS_ANTIGOS = "'tentativa_aberta', 'limite_3', 'execucao_cancelada'"
MOTIVOS_NOVOS = (
    f"{MOTIVOS_ANTIGOS}, 'health_check_ausente', 'health_check_reprovado', "
    "'health_check_expirado', 'dip_divergente'"
)


def upgrade() -> None:
    op.create_table(
        "health_check",
        sa.Column("hc_id", sa.UUID(), nullable=False),
        sa.Column("boot", sa.BigInteger(), nullable=True),
        sa.Column("aprovado", sa.Boolean(), nullable=True),
        sa.Column("tipo_dip", sa.String(), nullable=True),
        sa.Column("iniciado_em", sa.DateTime(timezone=True), nullable=False),
        sa.Column("concluido_em", sa.DateTime(timezone=True), nullable=True),
        sa.CheckConstraint(
            "tipo_dip IN ('4x4', '8x4', '12x4', 'invalido')", name="ck_health_check_tipo_dip"
        ),
        sa.PrimaryKeyConstraint("hc_id"),
    )
    op.add_column("tentativa", sa.Column("hc_id", sa.UUID(), nullable=True))
    op.create_foreign_key(
        "fk_tentativa_hc_id", "tentativa", "health_check", ["hc_id"], ["hc_id"]
    )
    op.add_column("health_check_item", sa.Column("hc_id", sa.UUID(), nullable=True))

    op.execute(
        """
        INSERT INTO health_check (hc_id, aprovado, tipo_dip, iniciado_em, concluido_em)
        SELECT t.tentativa_id, bool_and(i.aprovado), t.tipo_dip,
               min(i.verificado_em), max(i.verificado_em)
        FROM tentativa t JOIN health_check_item i ON i.tentativa_id = t.tentativa_id
        GROUP BY t.tentativa_id, t.tipo_dip
        """
    )
    op.execute("UPDATE health_check_item SET hc_id = tentativa_id")
    op.execute(
        "UPDATE tentativa SET hc_id = tentativa_id "
        "WHERE tentativa_id IN (SELECT hc_id FROM health_check)"
    )
    op.alter_column("health_check_item", "hc_id", nullable=False)
    op.create_foreign_key(
        "fk_health_check_item_hc_id", "health_check_item", "health_check", ["hc_id"], ["hc_id"]
    )
    op.drop_column("health_check_item", "tentativa_id")

    op.execute(
        "UPDATE tentativa SET status = 'failed', "
        "encerrada_em = COALESCE(encerrada_em, iniciada_em) "
        "WHERE status = 'health-check'"
    )
    op.drop_index("uq_tentativa_aberta_por_execucao", table_name="tentativa")
    op.create_index(
        "uq_tentativa_aberta_por_execucao",
        "tentativa",
        ["execucao_id"],
        unique=True,
        postgresql_where=sa.text("status = 'running'"),
    )
    op.drop_constraint("ck_tentativa_status", "tentativa", type_="check")
    op.create_check_constraint(
        "ck_tentativa_status", "tentativa", "status IN ('running', 'success', 'failed')"
    )

    op.drop_constraint("ck_tentativa_rejeitada_motivo", "tentativa_rejeitada", type_="check")
    op.create_check_constraint(
        "ck_tentativa_rejeitada_motivo", "tentativa_rejeitada", f"motivo IN ({MOTIVOS_NOVOS})"
    )


def downgrade() -> None:
    op.execute(
        "DELETE FROM tentativa_rejeitada "
        "WHERE motivo NOT IN (" + MOTIVOS_ANTIGOS + ")"
    )
    op.drop_constraint("ck_tentativa_rejeitada_motivo", "tentativa_rejeitada", type_="check")
    op.create_check_constraint(
        "ck_tentativa_rejeitada_motivo", "tentativa_rejeitada", f"motivo IN ({MOTIVOS_ANTIGOS})"
    )

    op.drop_constraint("ck_tentativa_status", "tentativa", type_="check")
    op.create_check_constraint(
        "ck_tentativa_status",
        "tentativa",
        "status IN ('health-check', 'running', 'success', 'failed')",
    )
    op.drop_index("uq_tentativa_aberta_por_execucao", table_name="tentativa")
    op.create_index(
        "uq_tentativa_aberta_por_execucao",
        "tentativa",
        ["execucao_id"],
        unique=True,
        postgresql_where=sa.text("status IN ('health-check', 'running')"),
    )

    # um health-check pode ter liberado mais de uma tentativa: os itens
    # voltam para a primeira; os de health-check sem tentativa se perdem
    op.add_column("health_check_item", sa.Column("tentativa_id", sa.UUID(), nullable=True))
    op.execute(
        """
        UPDATE health_check_item i SET tentativa_id = (
            SELECT t.tentativa_id FROM tentativa t
            WHERE t.hc_id = i.hc_id
            ORDER BY t.iniciada_em
            LIMIT 1
        )
        """
    )
    op.execute("DELETE FROM health_check_item WHERE tentativa_id IS NULL")
    op.alter_column("health_check_item", "tentativa_id", nullable=False)
    op.create_foreign_key(
        "health_check_item_tentativa_id_fkey",
        "health_check_item",
        "tentativa",
        ["tentativa_id"],
        ["tentativa_id"],
    )
    op.drop_column("health_check_item", "hc_id")
    op.drop_constraint("fk_tentativa_hc_id", "tentativa", type_="foreignkey")
    op.drop_column("tentativa", "hc_id")
    op.drop_table("health_check")
