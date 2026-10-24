import json
import logging
import uuid
from datetime import UTC, datetime, timedelta
from decimal import Decimal

import pytest
from app.models import (
    ExecucaoLogica,
    HealthCheck,
    Labirinto,
    LeituraTelemetria,
    ParedeCelula,
    Tentativa,
)
from app.repositories import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from app.services.monitor_conexao import MonitorConexao
from contrato.telemetria import Comando, ler_linha
from sqlalchemy import select
from sqlalchemy.exc import OperationalError

RECEBIDO_EM = "2026-10-08T12:00:00+00:00"
TEL = {
    "v": 1,
    "boot": 7,
    "seq": 42,
    "t_ms": 11250,
    "tipo": "tel",
    "estado": "running",
    "x": 0,
    "y": 2,
    "rumo": "N",
    "bat_mv": 7810,
    "vel_mm_s": 210,
    "eixo_longo": None,
}


def _linha(**campos) -> str:
    return json.dumps({**TEL, **campos}, separators=(",", ":"))


def _enviar(client, linha: str):
    return client.post("/telemetria", json={"linha": linha, "recebido_em": RECEBIDO_EM})


def _nova_execucao(db_session, tipo: str) -> ExecucaoLogica:
    labirinto_id = db_session.scalar(select(Labirinto.labirinto_id).where(Labirinto.tipo == tipo))
    execucao = ExecucaoLogica(
        execucao_id=uuid.uuid4(),
        labirinto_id=labirinto_id,
        status="em_andamento",
        tentativas_usadas=1,
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(execucao)
    return execucao


def _nova_tentativa(db_session, execucao: ExecucaoLogica, status: str) -> Tentativa:
    tentativa = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=1,
        status=status,
        tipo_inicio="nova",
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(tentativa)
    return tentativa


@pytest.fixture(params=["4x4", "8x4", "12x4"])
def tentativa_aberta(request, db_session) -> Tentativa:
    execucao = _nova_execucao(db_session, request.param)
    return _nova_tentativa(db_session, execucao, "running")


LINHAS_INVALIDAS = {
    "json_invalido": '{"v":1,"tipo":"tel"',
    "campo_faltando": json.dumps({k: v for k, v in TEL.items() if k != "bat_mv"}),
    "x_12": _linha(x=12),
    "versao_2": _linha(v=2),
    "300_bytes": _linha() + " " * (300 - len(_linha())),
}


@pytest.mark.parametrize("linha", LINHAS_INVALIDAS.values(), ids=LINHAS_INVALIDAS.keys())
def test_linha_invalida_da_422_e_a_proxima_valida_e_aceita(client, tentativa_aberta, caplog, linha):
    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, linha)

    assert resposta.status_code == 422
    assert resposta.json()["detail"]
    assert any(r.levelno == logging.WARNING for r in caplog.records)

    resposta = _enviar(client, _linha())
    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}


def test_linha_de_300_bytes_tem_mesmo_300_bytes():
    assert len(LINHAS_INVALIDAS["300_bytes"].encode()) == 300


def test_log_da_linha_invalida_trunca_em_256_caracteres(client, caplog):
    linha = "x" * 1000
    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, linha)

    assert resposta.status_code == 422
    registro = next(r for r in caplog.records if r.name == "app.telemetria")
    assert "x" * 256 in registro.getMessage()
    assert "x" * 257 not in registro.getMessage()


def test_sem_tentativa_aberta_descarta_com_200(client, db_session, caplog):
    execucao = _nova_execucao(db_session, "4x4")
    _nova_tentativa(db_session, execucao, "success")

    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        resposta = _enviar(client, _linha())

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    assert TelemetriaRepository(db_session).contar_leituras() == 0
    assert any(
        r.levelno == logging.INFO and "nenhuma tentativa aberta" in r.getMessage()
        for r in caplog.records
    )


def test_tentativa_aberta_e_a_mais_recente_com_labirinto(db_session):
    antiga = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    antiga.iniciada_em = datetime(2026, 10, 1, tzinfo=UTC)
    recente = _nova_tentativa(db_session, _nova_execucao(db_session, "8x4"), "running")
    _nova_tentativa(db_session, _nova_execucao(db_session, "12x4"), "failed")
    db_session.flush()

    aberta = Repositorio(db_session).buscar_tentativa_aberta()

    assert aberta is not None
    assert aberta.tentativa_id == recente.tentativa_id
    assert aberta.execucao.labirinto.tipo == "8x4"


def _leituras(db_session, tentativa: Tentativa) -> list[LeituraTelemetria]:
    stmt = (
        select(LeituraTelemetria)
        .where(LeituraTelemetria.tentativa_id == tentativa.tentativa_id)
        .order_by(LeituraTelemetria.ordem)
    )
    return list(db_session.scalars(stmt))


def _passo(seq: int, **campos) -> str:
    return json.dumps(
        {"v": 1, "boot": 7, "seq": seq, "t_ms": seq * 10, "tipo": "passo", "x": 0, "y": 1}
        | {"paredes": 5}
        | campos,
        separators=(",", ":"),
    )


@pytest.mark.parametrize(
    ("tipo", "x", "y", "status_esperado"),
    [
        ("4x4", 4, 0, 422),
        ("4x4", 3, 3, 200),
        ("8x4", 7, 2, 200),
        ("8x4", 2, 7, 200),
        ("8x4", 5, 5, 422),
        ("12x4", 11, 3, 200),
        ("12x4", 4, 4, 422),
    ],
)
def test_limites_do_labirinto(client, db_session, caplog, tipo, x, y, status_esperado):
    _nova_tentativa(db_session, _nova_execucao(db_session, tipo), "running")

    with caplog.at_level(logging.WARNING, logger="app.telemetria"):
        resposta = _enviar(client, _linha(x=x, y=y))

    assert resposta.status_code == status_esperado
    if status_esperado == 422:
        assert "fora do labirinto" in resposta.json()["detail"]
        assert TelemetriaRepository(db_session).contar_leituras() == 0
        assert any(r.levelno == logging.WARNING for r in caplog.records)


@pytest.mark.parametrize("campos", [{"tipo": "passo", "paredes": 0}, {"tipo": "sucesso"}])
def test_limites_valem_para_passo_e_sucesso(client, db_session, campos):
    _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    linha = json.dumps({"v": 1, "boot": 7, "seq": 1, "t_ms": 1, "x": 0, "y": 4} | campos)

    assert _enviar(client, linha).status_code == 422


def test_limites_valem_para_falha(client, db_session):
    _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    falha = {"tipo": "falha", "motivo": "stuck", "origem": "automatica", "componente": None}
    linha = json.dumps({"v": 1, "boot": 7, "seq": 1, "t_ms": 1, "x": 5, "y": 0} | falha)

    assert _enviar(client, linha).status_code == 422


def test_tel_grava_leitura_telemetria(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    # o robô ainda parado na largada manda a tel com o estado anterior
    _enviar(client, _linha(t_ms=1000, estado="health-check"))
    resposta = client.post(
        "/telemetria",
        json={"linha": _linha(t_ms=1500, x=1, estado="running"), "recebido_em": RECEBIDO_EM},
    )

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    primeira, segunda = _leituras(db_session, tentativa)
    assert (primeira.ordem, segunda.ordem) == (1, 2)
    # a fase é o status da tentativa, não o estado da tel
    assert primeira.fase == "running"
    assert (segunda.x, segunda.y) == (1, 2)
    assert (segunda.bateria, segunda.velocidade) == (Decimal("7810"), 210)
    assert segunda.rumo == "N"
    recebido_em = datetime.fromisoformat(RECEBIDO_EM)
    assert segunda.recebido_em == recebido_em
    # âncora fixada na primeira mensagem do boot: 12:00:00 - 1 s
    assert primeira.enviado_em == recebido_em
    assert segunda.enviado_em == recebido_em + timedelta(milliseconds=500)


def test_ordem_e_por_tentativa(client, db_session):
    antiga = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    _enviar(client, _linha())
    antiga.status = "failed"
    nova = _nova_tentativa(db_session, _nova_execucao(db_session, "8x4"), "running")
    db_session.flush()

    _enviar(client, _linha())

    assert [leitura.ordem for leitura in _leituras(db_session, nova)] == [1]
    assert [leitura.ordem for leitura in _leituras(db_session, antiga)] == [1]


def test_tel_com_mesmo_seq_nunca_e_repetida(client, tentativa_aberta, db_session):
    _enviar(client, _linha())
    _enviar(client, _linha())

    assert len(_leituras(db_session, tentativa_aberta)) == 2


def test_evento_repetido_devolve_200_e_loga_info(client, tentativa_aberta, caplog):
    assert _enviar(client, _passo(1)).status_code == 200

    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        resposta = _enviar(client, _passo(1))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    assert any(r.levelno == logging.INFO and "repetida" in r.getMessage() for r in caplog.records)


def test_seq_so_e_confirmado_depois_do_commit(client, db_session, monkeypatch, caplog):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    # fecha o savepoint: o rollback da ingestão não pode desfazer a tentativa
    db_session.commit()
    commit_original = db_session.commit

    def commit_falho():
        raise OperationalError("COMMIT", {}, Exception("banco caiu"))

    monkeypatch.setattr(db_session, "commit", commit_falho)
    with pytest.raises(OperationalError):
        _enviar(client, _passo(1))
    with pytest.raises(OperationalError):
        _enviar(client, _linha())
    assert _leituras(db_session, tentativa) == []

    monkeypatch.setattr(db_session, "commit", commit_original)
    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        assert _enviar(client, _passo(1)).status_code == 200
        assert _enviar(client, _linha()).status_code == 200

    assert not any("repetida" in r.getMessage() for r in caplog.records)
    assert [leitura.ordem for leitura in _leituras(db_session, tentativa)] == [1]
    assert client.app.state.deduplicador.eh_repetida(ler_linha(_passo(1)))


def test_ancora_so_e_fixada_depois_do_commit(client, db_session, monkeypatch):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    db_session.commit()
    commit_original = db_session.commit

    def commit_falho():
        raise OperationalError("COMMIT", {}, Exception("banco caiu"))

    monkeypatch.setattr(db_session, "commit", commit_falho)
    with pytest.raises(OperationalError):
        _enviar(client, _linha(t_ms=1000))

    monkeypatch.setattr(db_session, "commit", commit_original)
    assert _enviar(client, _linha(t_ms=1500)).status_code == 200

    # a âncora vem da mensagem aceita, não da que falhou no commit
    [leitura] = _leituras(db_session, tentativa)
    assert leitura.enviado_em == datetime.fromisoformat(RECEBIDO_EM)


def test_mensagem_valida_atualiza_monitor_de_conexao(client):
    monitor = client.app.state.monitor_conexao
    assert monitor.segundos_sem_sinal() is None

    _enviar(client, _linha())

    # usa o relógio do servidor, não o recebido_em da ponte (que pode ser antigo)
    assert monitor.segundos_sem_sinal() < 5


def test_link_lost_no_post_apos_silencio(client, db_session, tentativa_aberta):
    """RF32: gap medido antes de registrar_mensagem na linha que reconecta."""
    agora = datetime.now(UTC)
    tentativa_aberta.iniciada_em = agora - timedelta(seconds=20)
    db_session.commit()
    monitor = client.app.state.monitor_conexao
    monitor._ultima_mensagem = agora - timedelta(seconds=11)

    assert _enviar(client, _linha()).status_code == 200

    db_session.expire_all()
    execucao = db_session.get(ExecucaoLogica, tentativa_aberta.execucao_id)
    db_session.refresh(tentativa_aberta)
    assert tentativa_aberta.status == "failed"
    assert execucao.status == "em_andamento"
    assert tentativa_aberta.falha is not None
    assert tentativa_aberta.falha.motivo == "link_lost"


def test_primeira_telemetria_apos_reinicio_nao_fecha_tentativa_antiga(
    client, db_session, tentativa_aberta
):
    """#229: o monitor novo aguarda a ponte antes de medir ausência de sinal."""
    tentativa_aberta.iniciada_em = datetime.now(UTC) - timedelta(seconds=20)
    db_session.commit()
    monitor_novo = MonitorConexao(limite_sem_sinal_s=10)
    client.app.state.monitor_conexao = monitor_novo
    client.app.state.ingestao_telemetria.monitor = monitor_novo

    assert _enviar(client, _linha()).status_code == 200

    db_session.expire_all()
    tentativa = db_session.get(Tentativa, tentativa_aberta.tentativa_id)
    assert tentativa.status == "running"
    assert tentativa.falha is None
    assert len(_leituras(db_session, tentativa)) == 1


def _hc_item(seq: int, **campos) -> str:
    return json.dumps(
        {"v": 1, "boot": 7, "seq": seq, "t_ms": 1000 + seq * 100, "tipo": "hc_item"}
        | {"componente": "bateria", "aprovado": True, "valor": 7840}
        | campos,
        separators=(",", ":"),
    )


def _hc_resultado(seq: int, **campos) -> str:
    return json.dumps(
        {"v": 1, "boot": 7, "seq": seq, "t_ms": 1000 + seq * 100, "tipo": "hc_resultado"}
        | {"aprovado": True, "tipo_dip": "4x4", "inicio": "nova"}
        | campos,
        separators=(",", ":"),
    )


def _health_checks(db_session) -> list[HealthCheck]:
    return list(db_session.scalars(select(HealthCheck).order_by(HealthCheck.iniciado_em)))


def test_health_check_sem_tentativa_aberta_e_gravado(client, db_session):
    assert _enviar(client, _hc_item(0)).status_code == 200
    _enviar(client, _hc_item(1, componente="motor_esquerdo", aprovado=False, valor=None))
    resposta = _enviar(client, _hc_resultado(2, aprovado=False))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    [health_check] = _health_checks(db_session)
    assert (health_check.boot, health_check.aprovado, health_check.tipo_dip) == (
        7,
        False,
        "4x4",
    )
    assert health_check.concluido_em is not None
    bateria, motor = health_check.itens
    assert (bateria.componente, bateria.aprovado, bateria.valor_lido) == ("bateria", True, 7840)
    assert (motor.componente, motor.aprovado, motor.valor_lido) == ("motor_esquerdo", False, None)
    recebido_em = datetime.fromisoformat(RECEBIDO_EM)
    assert bateria.verificado_em == recebido_em
    assert motor.verificado_em == recebido_em + timedelta(milliseconds=100)


def test_novo_health_check_depois_do_resultado_comeca_outro_registro(client, db_session):
    _enviar(client, _hc_item(0))
    _enviar(client, _hc_resultado(1, aprovado=False))
    _enviar(client, _hc_item(2))
    _enviar(client, _hc_resultado(3))

    primeiro, segundo = _health_checks(db_session)
    assert (primeiro.aprovado, len(primeiro.itens)) == (False, 1)
    assert (segundo.aprovado, len(segundo.itens)) == (True, 1)


HEALTH_CHECK_CMD = {"v": 1, "cmd": "health_check"}


def test_pedido_de_health_check_vai_nas_respostas_ate_o_robo_comecar(client, db_session):
    pedido = client.post("/health-check/solicitar")
    assert pedido.status_code == 202
    assert "solicitado_em" in pedido.json()

    parado = _enviar(client, _linha(seq=1, estado="health-check"))
    assert parado.json() == {"comandos": [HEALTH_CHECK_CMD]}
    repetido = _enviar(client, _linha(seq=2, estado="health-check"))
    assert repetido.json() == {"comandos": [HEALTH_CHECK_CMD]}

    assert _enviar(client, _hc_item(3)).json() == {"comandos": []}
    assert _enviar(client, _hc_resultado(4)).json() == {"comandos": []}
    assert len(_health_checks(db_session)) == 1


def test_pedido_de_health_check_recusado_com_tentativa_em_execucao(client, db_session):
    _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    resposta = client.post("/health-check/solicitar")

    assert resposta.status_code == 409
    assert "tentativa em execução" in resposta.json()["detail"]
    assert _enviar(client, _linha(seq=1)).json() == {"comandos": []}


def test_health_check_nao_mexe_na_tentativa_aberta(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    _enviar(client, _hc_item(0))
    _enviar(client, _hc_resultado(1, aprovado=False))

    db_session.refresh(tentativa)
    assert tentativa.status == "running"
    assert tentativa.hc_id is None
    assert len(_health_checks(db_session)) == 1


def _parede(db_session, execucao_id, x: int, y: int) -> ParedeCelula:
    stmt = select(ParedeCelula).where(
        ParedeCelula.execucao_id == execucao_id, ParedeCelula.x == x, ParedeCelula.y == y
    )
    return db_session.scalars(stmt).one()


def test_passo_grava_passo_trajeto_e_parede_celula(client, tentativa_aberta, db_session):
    assert _enviar(client, _passo(0, x=0, y=0, paredes=10)).status_code == 200
    resposta = _enviar(client, _passo(1, x=0, y=1, paredes=5))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    execucao_id = tentativa_aberta.execucao_id
    primeiro, segundo = TelemetriaRepository(db_session).listar_trajeto(execucao_id)
    assert (primeiro.seq, primeiro.x, primeiro.y, primeiro.paredes_mask) == (1, 0, 0, 10)
    assert (segundo.seq, segundo.x, segundo.y, segundo.paredes_mask) == (2, 0, 1, 5)
    assert segundo.tentativa_id == tentativa_aberta.tentativa_id
    assert not primeiro.retomada
    recebido_em = datetime.fromisoformat(RECEBIDO_EM)
    assert primeiro.entrou_em == recebido_em
    assert segundo.entrou_em == recebido_em + timedelta(milliseconds=10)
    parede = _parede(db_session, execucao_id, 0, 0)
    # 10 = sul (2) + oeste (8)
    assert (parede.norte, parede.sul, parede.leste, parede.oeste) == (False, True, False, True)
    assert parede.detectada_em == recebido_em


def test_beco_gera_trajeto_com_volta_e_parede_da_ultima_leitura(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    passos = [(0, 0, 2), (0, 1, 0), (0, 2, 0), (0, 2, 13), (0, 1, 0)]
    for seq, (x, y, paredes) in enumerate(passos):
        assert _enviar(client, _passo(seq, x=x, y=y, paredes=paredes)).status_code == 200

    trajeto = TelemetriaRepository(db_session).listar_trajeto(tentativa.execucao_id)

    assert [(p.seq, p.x, p.y) for p in trajeto] == [(1, 0, 0), (2, 0, 1), (3, 0, 2), (4, 0, 1)]
    assert trajeto[2].paredes_mask == 13
    parede = _parede(db_session, tentativa.execucao_id, 0, 2)
    # 13 = norte (1) + leste (4) + oeste (8)
    assert (parede.norte, parede.sul, parede.leste, parede.oeste) == (True, False, True, True)


def test_ultima_leitura_da_parede_vence(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    _enviar(client, _passo(0, x=1, y=1, paredes=9))
    _enviar(client, _passo(1, x=1, y=2, paredes=0))
    _enviar(client, _passo(2, x=1, y=1, paredes=1))

    parede = _parede(db_session, tentativa.execucao_id, 1, 1)
    assert (parede.norte, parede.oeste) == (True, False)


def test_seq_do_passo_e_por_execucao_e_retomada_so_no_primeiro(client, db_session):
    execucao = _nova_execucao(db_session, "8x4")
    primeira = _nova_tentativa(db_session, execucao, "running")
    _enviar(client, _passo(0, x=0, y=0))
    _enviar(client, _passo(1, x=0, y=1))
    primeira.status = "failed"
    db_session.flush()
    segunda = Tentativa(
        tentativa_id=uuid.uuid4(),
        execucao_id=execucao.execucao_id,
        attempt_index=2,
        status="running",
        tipo_inicio="retomada",
        iniciada_em=datetime.now(UTC),
    )
    Repositorio(db_session).salvar(segunda)

    # a retomada começa na célula da falha: ganha o próprio passo, marcado
    _enviar(client, _passo(2, x=0, y=1, paredes=3))
    _enviar(client, _passo(3, x=0, y=1, paredes=1))  # releitura na mesma tentativa
    _enviar(client, _passo(4, x=0, y=2))

    trajeto = TelemetriaRepository(db_session).listar_trajeto(execucao.execucao_id)
    assert [(p.seq, p.x, p.y) for p in trajeto] == [(1, 0, 0), (2, 0, 1), (3, 0, 1), (4, 0, 2)]
    assert [p.tentativa_id for p in trajeto] == [primeira.tentativa_id] * 2 + [
        segunda.tentativa_id
    ] * 2
    assert [p.retomada for p in trajeto] == [False, False, True, False]
    assert trajeto[1].paredes_mask == 5
    assert trajeto[2].paredes_mask == 1


def test_passo_repetido_nao_grava_de_novo(client, tentativa_aberta, db_session):
    _enviar(client, _passo(1, x=0, y=0))
    _enviar(client, _passo(2, x=0, y=1))
    _enviar(client, _passo(1, x=0, y=0))

    trajeto = TelemetriaRepository(db_session).listar_trajeto(tentativa_aberta.execucao_id)
    assert [(p.x, p.y) for p in trajeto] == [(0, 0), (0, 1)]


def test_passo_e_parede_na_mesma_transacao(client, db_session, monkeypatch):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")
    execucao_id = tentativa.execucao_id
    # fecha o savepoint: o rollback da ingestão não pode desfazer a tentativa
    db_session.commit()

    def upsert_falho(*_args):
        raise OperationalError("INSERT", {}, Exception("banco caiu"))

    monkeypatch.setattr(TelemetriaRepository, "gravar_paredes", upsert_falho)
    with pytest.raises(OperationalError):
        _enviar(client, _passo(1, x=0, y=0))

    assert TelemetriaRepository(db_session).listar_trajeto(execucao_id) == []


class GerenciadorFalso:
    """Registra os eventos recebidos e devolve comandos fixos."""

    def __init__(self, ordem: list[str]) -> None:
        self.ordem = ordem
        self.eventos = []
        self.comandos = [Comando()]
        self.validade_health_check_s = 420

    def aplicar_evento(self, sessao, tentativa, mensagem, *, momento=None) -> None:
        self.ordem.append("aplicar_evento")
        self.eventos.append((sessao, tentativa, mensagem))

    def comandos_pendentes(self) -> list[Comando]:
        return self.comandos

    def confirmar_largada(self) -> None:
        self.ordem.append("confirmar_largada")

    def confirmar_health_check(self) -> None:
        self.ordem.append("confirmar_health_check")

    def verificar_monitores(self, sessao, monitor) -> bool:
        return False


@pytest.fixture
def gerenciador_falso(client, db_session, monkeypatch) -> GerenciadorFalso:
    ordem = []
    commit_original = db_session.commit

    def commit_registrado():
        ordem.append("commit")
        commit_original()

    monkeypatch.setattr(db_session, "commit", commit_registrado)
    gerenciador = GerenciadorFalso(ordem)
    monkeypatch.setattr(client.app.state, "gerenciador", gerenciador)
    return gerenciador


EVENTOS = {
    "falha": {
        "tipo": "falha",
        "motivo": "stuck",
        "origem": "automatica",
        "x": 1,
        "y": 2,
        "componente": None,
    },
    "sucesso": {"tipo": "sucesso", "x": 3, "y": 3},
}


def _evento(seq: int, campos: dict) -> str:
    return json.dumps({"v": 1, "boot": 7, "seq": seq, "t_ms": 1000 + seq} | campos)


@pytest.mark.parametrize("campos", EVENTOS.values(), ids=EVENTOS.keys())
def test_evento_vai_ao_gerenciador_antes_do_commit(client, db_session, gerenciador_falso, campos):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    resposta = _enviar(client, _evento(1, campos))
    _enviar(client, _evento(1, campos))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": [{"v": 1, "cmd": "interromper"}]}
    # o repetido não chega de novo ao gerenciador
    [(sessao, recebida, mensagem)] = gerenciador_falso.eventos
    assert sessao is db_session
    assert recebida.tentativa_id == tentativa.tentativa_id
    assert mensagem == ler_linha(_evento(1, campos))
    # Sem cancelamento, verificar_monitores não confirma transação: o POST só
    # commita o próprio evento, e aplicar_evento continua antes desse commit.
    # A repetida não chega ao gerenciador nem gera outro commit.
    assert gerenciador_falso.ordem == ["aplicar_evento", "commit"]


def test_falha_web_sem_tentativa_aberta_chega_com_none(client, gerenciador_falso):
    falha_web = EVENTOS["falha"] | {"origem": "web", "motivo": None}

    resposta = _enviar(client, _evento(1, falha_web))

    assert resposta.status_code == 200
    [(_, tentativa, mensagem)] = gerenciador_falso.eventos
    assert tentativa is None
    assert mensagem.origem == "web"


@pytest.mark.parametrize("campos", EVENTOS.values(), ids=EVENTOS.keys())
def test_evento_sem_tentativa_aberta_e_descartado(client, gerenciador_falso, campos):
    resposta = _enviar(client, _evento(1, campos))

    assert resposta.status_code == 200
    assert gerenciador_falso.eventos == []


def test_comandos_do_gerenciador_vao_na_resposta_da_tel(
    client, tentativa_aberta, gerenciador_falso
):
    resposta = _enviar(client, _linha())

    assert resposta.json() == {"comandos": [{"v": 1, "cmd": "interromper"}]}
    assert gerenciador_falso.eventos == []


@pytest.mark.parametrize(
    ("linha", "confirma"),
    [
        (_linha(estado="running"), True),
        (_linha(estado="health-check"), False),
        (_passo(1), True),
    ],
    ids=["tel_running", "tel_parado", "passo"],
)
def test_largada_confirmada_pela_tel_running_ou_passo(
    client, tentativa_aberta, gerenciador_falso, linha, confirma
):
    _enviar(client, linha)

    assert ("confirmar_largada" in gerenciador_falso.ordem) is confirma


def test_hc_resultado_nao_vai_ao_gerenciador(client, tentativa_aberta, gerenciador_falso):
    hc = {"tipo": "hc_resultado", "aprovado": True, "tipo_dip": "4x4", "inicio": "nova"}
    _enviar(client, _evento(1, hc))

    assert gerenciador_falso.eventos == []


def test_linha_invalida_tambem_leva_os_comandos(client, gerenciador_falso):
    resposta = _enviar(client, "{nao é json")

    assert resposta.status_code == 422
    assert resposta.json()["comandos"] == [{"v": 1, "cmd": "interromper"}]
    assert resposta.json()["detail"]
