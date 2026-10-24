"""Retrato do health-check para a stream que existe antes da execução."""

from datetime import timedelta

from app.models import HealthCheck
from app.schemas.api import EventoHealthCheckAtual
from app.services.consulta_execucoes import itens_health_check
from app.services.publicador_sse import EventoSSE

CANAL_HEALTH_CHECK = "health-check"
# o GET inicial abre a stream; daí em diante o relógio do keep-alive é de 1 s
INTERVALO_RELOGIO_S = 1.0


def evento_health_check(health_check: HealthCheck, validade_s: int) -> EventoSSE:
    valido_ate = None
    if health_check.aprovado and health_check.concluido_em is not None:
        valido_ate = health_check.concluido_em + timedelta(seconds=validade_s)
    corpo = EventoHealthCheckAtual(
        aprovado=health_check.aprovado,
        tipo_dip=health_check.tipo_dip,
        iniciado_em=health_check.iniciado_em,
        concluido_em=health_check.concluido_em,
        valido_ate=valido_ate,
        itens=itens_health_check(health_check),
    )
    return EventoSSE(tipo=corpo.tipo, dados=corpo.model_dump_json())
