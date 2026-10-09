"""Fábrica de execuções completas para os testes de consulta (BACK-06)."""

import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

import pytest
from app.models import (
    ExecucaoLogica,
    Falha,
    HealthCheckItem,
    Labirinto,
    LeituraTelemetria,
    ParedeCelula,
    PassoTrajeto,
    Tentativa,
)
from sqlalchemy import select
from sqlalchemy.orm import Session

COMPONENTES = [
    "bateria",
    "tof_frontal_esq",
    "tof_frontal_dir",
    "tof_esquerdo",
    "tof_direito",
    "motor_esquerdo",
    "motor_direito",
    "encoder_esquerdo",
    "encoder_direito",
]
INICIO = datetime(2026, 10, 7, 17, 0, tzinfo=UTC)


class Fabrica:
    def __init__(self, session: Session) -> None:
        self.session = session

    def execucao(
        self,
        *,
        tipo: str = "4x4",
        status: str = "concluida",
        iniciada_em: datetime = INICIO,
        tentativas_usadas: int = 0,
        tempo_total_s: Decimal | None = None,
    ) -> ExecucaoLogica:
        execucao = ExecucaoLogica(
            execucao_id=uuid.uuid4(),
            labirinto_id=self.session.scalar(
                select(Labirinto.labirinto_id).where(Labirinto.tipo == tipo)
            ),
            status=status,
            tentativas_usadas=tentativas_usadas,
            iniciada_em=iniciada_em,
            encerrada_em=None if status == "em_andamento" else iniciada_em + timedelta(minutes=2),
            tempo_total_s=tempo_total_s,
        )
        self.session.add(execucao)
        self.session.flush()
        return execucao

    def tentativa(
        self,
        execucao: ExecucaoLogica,
        *,
        attempt_index: int = 1,
        status: str = "success",
        tempo_s: Decimal | None = None,
        velocidade_media: Decimal | None = None,
        bateria_inicial: Decimal | None = Decimal("8.2"),
        bateria_final: Decimal | None = Decimal("8.1"),
        consumo_bateria: Decimal | None = None,
    ) -> Tentativa:
        iniciada_em = execucao.iniciada_em + timedelta(seconds=40 * (attempt_index - 1))
        aberta = status in ("health-check", "running")
        tentativa = Tentativa(
            tentativa_id=uuid.uuid4(),
            execucao_id=execucao.execucao_id,
            attempt_index=attempt_index,
            status=status,
            tipo_inicio="nova" if attempt_index == 1 else "retomada",
            tipo_dip=execucao.labirinto.tipo,
            iniciada_em=iniciada_em,
            encerrada_em=None if aberta else iniciada_em + timedelta(seconds=35),
            tempo_s=tempo_s,
            velocidade_media=velocidade_media,
            bateria_inicial=bateria_inicial,
            bateria_final=None if aberta else bateria_final,
            consumo_bateria=consumo_bateria,
        )
        self.session.add(tentativa)
        self.session.flush()
        return tentativa

    def health_check(self, tentativa: Tentativa, *, reprovado: str | None = None) -> None:
        for i, componente in enumerate(COMPONENTES):
            self.session.add(
                HealthCheckItem(
                    tentativa_id=tentativa.tentativa_id,
                    componente=componente,
                    aprovado=componente != reprovado,
                    valor_lido=Decimal("8.2") if componente == "bateria" else None,
                    verificado_em=tentativa.iniciada_em + timedelta(milliseconds=10 * i),
                )
            )
        self.session.flush()

    def falha(
        self,
        tentativa: Tentativa,
        *,
        motivo: str = "collision",
        origem: str = "automatica",
        celula: tuple[int, int] | None = (2, 1),
    ) -> Falha:
        falha = Falha(
            tentativa_id=tentativa.tentativa_id,
            motivo=motivo,
            origem=origem,
            celula_x=celula[0] if celula else None,
            celula_y=celula[1] if celula else None,
            momento_falha=tentativa.iniciada_em + timedelta(seconds=30),
        )
        self.session.add(falha)
        self.session.flush()
        return falha

    def passos(
        self,
        tentativa: Tentativa,
        celulas: list[tuple[int, int]],
        *,
        seq_inicial: int = 1,
        paredes_mask: int | None = 5,
    ) -> list[PassoTrajeto]:
        passos = [
            PassoTrajeto(
                tentativa_id=tentativa.tentativa_id,
                execucao_id=tentativa.execucao_id,
                seq=seq_inicial + i,
                x=x,
                y=y,
                paredes_mask=paredes_mask,
                retomada=tentativa.tipo_inicio == "retomada" and i == 0,
                entrou_em=tentativa.iniciada_em + timedelta(milliseconds=300 * i),
            )
            for i, (x, y) in enumerate(celulas)
        ]
        self.session.add_all(passos)
        self.session.flush()
        return passos

    def parede(self, execucao: ExecucaoLogica, x: int, y: int, **lados: bool) -> None:
        self.session.add(
            ParedeCelula(
                execucao_id=execucao.execucao_id,
                x=x,
                y=y,
                detectada_em=execucao.iniciada_em,
                **lados,
            )
        )
        self.session.flush()

    def leituras(
        self,
        tentativa: Tentativa,
        quantidade: int,
        *,
        celulas: list[tuple[int, int]] | None = None,
    ) -> list[LeituraTelemetria]:
        celulas = celulas or [(0, 0)]
        leituras = [
            LeituraTelemetria(
                tentativa_id=tentativa.tentativa_id,
                ordem=i + 1,
                fase="running",
                x=celulas[i % len(celulas)][0],
                y=celulas[i % len(celulas)][1],
                bateria=Decimal("8.15"),
                velocidade=Decimal("0.21"),
                enviado_em=tentativa.iniciada_em + timedelta(milliseconds=100 * i),
                recebido_em=tentativa.iniciada_em + timedelta(milliseconds=100 * i + 20),
            )
            for i in range(quantidade)
        ]
        self.session.add_all(leituras)
        self.session.flush()
        return leituras

    def execucao_12x4_com_3_tentativas(
        self, *, passos_por_tentativa: int = 60, leituras_por_tentativa: int = 300
    ) -> ExecucaoLogica:
        """Duas falhas e uma conclusão no 12x4, com trajeto, paredes e leituras."""
        execucao = self.execucao(tipo="12x4", tentativas_usadas=3, tempo_total_s=Decimal("118.4"))
        celulas = [(x, y) for y in range(4) for x in range(12)]
        seq = 1
        for indice, status in enumerate(["failed", "failed", "success"], start=1):
            tentativa = self.tentativa(
                execucao,
                attempt_index=indice,
                status=status,
                tempo_s=Decimal("41.7") if status == "success" else None,
                velocidade_media=Decimal("0.19"),
            )
            self.health_check(tentativa)
            caminho = [celulas[i % len(celulas)] for i in range(passos_por_tentativa)]
            self.passos(tentativa, caminho, seq_inicial=seq)
            seq += passos_por_tentativa
            self.leituras(tentativa, leituras_por_tentativa, celulas=caminho)
            if status == "failed":
                self.falha(tentativa, celula=caminho[-1])
        for x, y in celulas:
            self.parede(execucao, x, y, norte=y == 3, sul=y == 0)
        self.session.expire_all()
        return execucao


@pytest.fixture
def fabrica(db_session: Session) -> Fabrica:
    return Fabrica(db_session)
