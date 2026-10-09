import json
import logging
import uuid
from datetime import UTC, datetime, timedelta

import pytest
from app.models import (
    ExecucaoLogica,
    HealthCheckItem,
    Labirinto,
    LeituraTelemetria,
    ParedeCelula,
    Tentativa,
)
from app.repositories import Repositorio
from app.repositories.telemetria import TelemetriaRepository
from contrato.telemetria import ler_linha
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
    recente = _nova_tentativa(db_session, _nova_execucao(db_session, "8x4"), "health-check")
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
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "health-check")

    _enviar(client, _linha(t_ms=1000))
    resposta = client.post(
        "/telemetria",
        json={"linha": _linha(t_ms=1500, x=1, estado="running"), "recebido_em": RECEBIDO_EM},
    )

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    primeira, segunda = _leituras(db_session, tentativa)
    assert (primeira.ordem, segunda.ordem) == (1, 2)
    # a fase é o status da tentativa, não o estado da tel
    assert segunda.fase == "health-check"
    assert (segunda.x, segunda.y) == (1, 2)
    assert (segunda.bateria, segunda.velocidade) == (7810, 210)
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


def test_mensagem_valida_atualiza_monitor_de_conexao(client):
    monitor = client.app.state.monitor_conexao
    assert monitor.segundos_sem_sinal() is None

    _enviar(client, _linha())

    agora = datetime.fromisoformat(RECEBIDO_EM) + timedelta(seconds=3)
    assert monitor.segundos_sem_sinal(agora) == 3


def _hc_item(seq: int, **campos) -> str:
    return json.dumps(
        {"v": 1, "boot": 7, "seq": seq, "t_ms": 1000 + seq * 100, "tipo": "hc_item"}
        | {"componente": "bateria", "aprovado": True, "valor": 7840}
        | campos,
        separators=(",", ":"),
    )


def _itens(db_session, tentativa: Tentativa) -> list[HealthCheckItem]:
    stmt = (
        select(HealthCheckItem)
        .where(HealthCheckItem.tentativa_id == tentativa.tentativa_id)
        .order_by(HealthCheckItem.item_id)
    )
    return list(db_session.scalars(stmt))


def test_hc_item_em_health_check_grava_health_check_item(client, db_session):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "health-check")

    assert _enviar(client, _hc_item(0)).status_code == 200
    resposta = _enviar(client, _hc_item(1, componente="motor_esquerdo", aprovado=False, valor=None))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    bateria, motor = _itens(db_session, tentativa)
    assert (bateria.componente, bateria.aprovado, bateria.valor_lido) == ("bateria", True, 7840)
    assert (motor.componente, motor.aprovado, motor.valor_lido) == ("motor_esquerdo", False, None)
    recebido_em = datetime.fromisoformat(RECEBIDO_EM)
    assert bateria.verificado_em == recebido_em
    assert motor.verificado_em == recebido_em + timedelta(milliseconds=100)


def test_hc_item_em_running_nao_grava_e_loga_info(client, db_session, caplog):
    tentativa = _nova_tentativa(db_session, _nova_execucao(db_session, "4x4"), "running")

    with caplog.at_level(logging.INFO, logger="app.telemetria"):
        resposta = _enviar(client, _hc_item(0))

    assert resposta.status_code == 200
    assert resposta.json() == {"comandos": []}
    assert _itens(db_session, tentativa) == []
    assert any(
        r.levelno == logging.INFO and "hc_item descartado" in r.getMessage() for r in caplog.records
    )


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

    # mesma célula do último passo da execução: só atualiza as paredes
    _enviar(client, _passo(2, x=0, y=1, paredes=3))
    _enviar(client, _passo(3, x=0, y=2))
    _enviar(client, _passo(4, x=0, y=3))

    trajeto = TelemetriaRepository(db_session).listar_trajeto(execucao.execucao_id)
    assert [p.seq for p in trajeto] == [1, 2, 3, 4]
    assert [p.tentativa_id for p in trajeto] == [primeira.tentativa_id] * 2 + [
        segunda.tentativa_id
    ] * 2
    assert [p.retomada for p in trajeto] == [False, False, True, False]
    assert trajeto[1].paredes_mask == 3


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
