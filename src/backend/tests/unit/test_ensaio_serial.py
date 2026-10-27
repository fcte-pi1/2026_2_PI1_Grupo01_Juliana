from datetime import datetime, timedelta, timezone

import pytest
from scripts.ensaio_serial import (
    EventoPonte,
    Janela,
    ResultadoBanco,
    conferir_ponte,
    contar_janelas,
    gravar_csv_janelas,
    ler_log_ponte,
    ler_log_simulador,
    relatorio,
)

TEL = '{"v":1,"boot":7,"seq":0,"t_ms":0,"tipo":"tel"}'
PASSO = '{"v":1,"boot":7,"seq":1,"t_ms":200,"tipo":"passo"}'
FUSO = timezone(timedelta(hours=-3))


def test_ler_log_simulador_devolve_so_as_enviadas_em_ordem(tmp_path):
    caminho = tmp_path / "simulador.log"
    caminho.write_text(
        "simulador: abrindo pty /dev/pts/5\n"
        f"simulador: enviada: {TEL}\n"
        "Traceback (most recent call last):\n"
        f"simulador: enviada: {PASSO}\n"
        "simulador: roteiro estouro-10min terminado: 2 linhas\n",
        encoding="utf-8",
    )

    assert ler_log_simulador(caminho) == [TEL, PASSO]


def test_ler_log_ponte_le_cada_tipo(tmp_path):
    caminho = tmp_path / "ponte.log"
    caminho.write_text(
        "2026-10-25T16:00:00.000-03:00 CONEXAO aberta /dev/pts/5\n"
        f"2026-10-25T16:00:00.250-03:00 RX {PASSO}\n"
        "2026-10-25T16:00:00.260-03:00 API 200 comandos=1\n"
        '2026-10-25T16:00:00.261-03:00 TX {"v":1,"cmd":"largar"}\n'
        f"2026-10-25T16:00:01.000-03:00 FILA descartada tel {TEL}\n",
        encoding="utf-8",
    )

    eventos = ler_log_ponte(caminho)

    assert [e.tipo for e in eventos] == ["CONEXAO", "RX", "API", "TX", "FILA"]
    assert eventos[1] == EventoPonte(
        hora=datetime(2026, 10, 25, 16, 0, 0, 250000, tzinfo=FUSO), tipo="RX", conteudo=PASSO
    )
    assert eventos[2].conteudo == "200 comandos=1"
    assert eventos[4].conteudo == f"descartada tel {TEL}"
    assert all(e.hora.tzinfo is not None for e in eventos)


def test_ler_log_ponte_recusa_linha_fora_do_formato(tmp_path):
    caminho = tmp_path / "ponte.log"
    caminho.write_text("2026-10-25T16:00:00.000-03:00 XYZ algo\n", encoding="utf-8")

    with pytest.raises(ValueError, match="linha 1"):
        ler_log_ponte(caminho)


INICIO = datetime(2026, 10, 25, 16, 0, 0, tzinfo=FUSO)
ENVIADAS = [
    TEL,
    PASSO,
    '{"v":1,"boot":7,"seq":2,"t_ms":400,"tipo":"tel"}',
    '{"v":1,"boot":7,"seq":3,"t_ms":600,"tipo":"tel"}',
]


def _eventos(rx, api=None, extras=()):
    """RX de cada linha seguido da resposta da API (200 por padrão), e os extras no fim."""
    eventos = [EventoPonte(INICIO, "CONEXAO", "aberta /dev/pts/5")]
    for indice, linha in enumerate(rx):
        hora = INICIO + timedelta(milliseconds=200 * (indice + 1))
        eventos.append(EventoPonte(hora, "RX", linha))
        resposta = api[indice] if api else "200 comandos=0"
        eventos.append(EventoPonte(hora + timedelta(milliseconds=5), "API", resposta))
    fim = INICIO + timedelta(seconds=10)
    eventos.extend(EventoPonte(fim, tipo, conteudo) for tipo, conteudo in extras)
    return eventos


def test_conferir_ponte_caso_ideal_aprova():
    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:]))

    assert resultado.aprovado
    assert resultado.problemas == []
    assert (resultado.enviadas, resultado.rx, resultado.api_200) == (4, 3, 3)
    assert (resultado.api_outro_status, resultado.api_falhou, resultado.fila_descartada) == (
        0,
        0,
        0,
    )


def test_conferir_ponte_aprova_tx_de_largar_e_health_check():
    tx = [("TX", '{"v":1,"cmd":"largar"}'), ("TX", '{"v":1,"cmd":"health_check"}')]

    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], extras=tx))

    assert resultado.aprovado
    assert resultado.tx == ['{"v":1,"cmd":"largar"}', '{"v":1,"cmd":"health_check"}']
    assert resultado.tx_invalidos == []


def test_conferir_ponte_reprova_linha_faltando_no_rx():
    rx = [ENVIADAS[1], ENVIADAS[3]]

    resultado = conferir_ponte(ENVIADAS, _eventos(rx))

    assert not resultado.aprovado
    assert "enviadas (4) diferente de RX + 1 (3)" in resultado.problemas
    assert any(p.startswith("RX 2 diferente da enviada 3") for p in resultado.problemas)


def test_conferir_ponte_reprova_linha_alterada():
    rx = [ENVIADAS[1], ENVIADAS[2].replace('"seq":2', '"seq":9'), ENVIADAS[3]]

    resultado = conferir_ponte(ENVIADAS, _eventos(rx))

    assert not resultado.aprovado
    assert len(resultado.problemas) == 1
    assert resultado.problemas[0].startswith("RX 2 diferente da enviada 3")


def test_conferir_ponte_reprova_api_422():
    api = ["200 comandos=0", "422 comandos=0", "200 comandos=0"]

    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], api=api))

    assert not resultado.aprovado
    assert resultado.api_outro_status == 1
    assert resultado.problemas == ["1 respostas da API com status 422"]


def test_conferir_ponte_reprova_api_falhou():
    falhou = [("API", "falhou ConnectError conexão recusada")]

    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], extras=falhou))

    assert not resultado.aprovado
    assert resultado.api_falhou == 1
    assert resultado.problemas == ["1 API falhou, a primeira: falhou ConnectError conexão recusada"]


def test_conferir_ponte_reprova_fila_descartada():
    fila = [("FILA", f"descartada tel {TEL}")]

    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], extras=fila))

    assert not resultado.aprovado
    assert resultado.fila_descartada == 1
    assert resultado.problemas == [f"1 FILA descartada, a primeira: descartada tel {TEL}"]


def test_conferir_ponte_reprova_tx_fora_do_contrato():
    tx = [("TX", '{"v":1,"cmd":"pular"}')]

    resultado = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], extras=tx))

    assert not resultado.aprovado
    assert resultado.tx_invalidos == ['{"v":1,"cmd":"pular"}']
    assert resultado.problemas == ['TX fora do contrato: {"v":1,"cmd":"pular"}']


def test_conferir_ponte_reprova_resposta_da_api_faltando():
    eventos = _eventos(ENVIADAS[1:])
    del eventos[-1]

    resultado = conferir_ponte(ENVIADAS, eventos)

    assert resultado.problemas == ["respostas da API (2) diferente de RX (3)"]


def _rx_em(segundos):
    return [EventoPonte(INICIO + timedelta(seconds=s), "RX", TEL) for s in segundos]


def test_contar_janelas_por_10_s_desde_o_primeiro_rx():
    eventos = _rx_em([0, 4, 12, 25])
    enviadas = [TEL] * 5
    gravadas = [INICIO + timedelta(seconds=s) for s in (4.1, 12.1, 12.2)]

    janelas = contar_janelas(enviadas, eventos, gravadas)

    assert janelas == [
        Janela(inicio_s=0, enviadas=3, rx=2, gravadas=1),
        Janela(inicio_s=10, enviadas=1, rx=1, gravadas=2),
        Janela(inicio_s=20, enviadas=1, rx=1, gravadas=0),
    ]


def test_contar_janelas_enviada_sem_rx_fica_no_ultimo_rx():
    janelas = contar_janelas([TEL] * 4, _rx_em([0, 15]), [])

    assert [(j.enviadas, j.rx) for j in janelas] == [(2, 1), (2, 1)]


def test_contar_janelas_sem_rx_devolve_vazio():
    assert contar_janelas([TEL], [], []) == []


def test_gravar_csv_janelas(tmp_path):
    caminho = tmp_path / "janelas.csv"

    gravar_csv_janelas(caminho, [Janela(0, 51, 50, 49), Janela(10, 50, 50, 50)])

    assert caminho.read_text(encoding="utf-8").splitlines() == [
        "janela_inicio_s,enviadas,rx,gravadas",
        "0,51,50,49",
        "10,50,50,50",
    ]


def _banco(problemas=()):
    return ResultadoBanco(
        execucao_id=None,
        motivo_encerramento="time_exceeded",
        antes={"hc_item": 8},
        dentro={"tel": 3, "passo": 1},
        depois={},
        gravados={"tel": 3, "passo": 1, "hc_item": 8},
        fronteira=[],
        problemas=list(problemas),
    )


def test_relatorio_aprova_com_600_s_e_conta_tx_por_comando():
    tx = [("TX", '{"v":1,"cmd":"largar"}'), ("TX", '{"v":1,"cmd":"largar"}')]
    ponte = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:], extras=tx))

    linhas, aprovado = relatorio(ponte, _banco(), 600.0)

    assert aprovado
    assert linhas[-1] == "APROVADO"
    assert "TX por comando: largar=2" in linhas
    assert "motivo do encerramento: time_exceeded" in linhas


def test_relatorio_reprova_ensaio_curto():
    ponte = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:]))

    linhas, aprovado = relatorio(ponte, _banco(), 599.9)

    assert not aprovado
    assert linhas[-1] == "REPROVADO"
    assert "  ensaio de 599.9 s, menor que 600 s" in linhas


def test_relatorio_reprova_com_problema_do_banco():
    ponte = conferir_ponte(ENVIADAS, _eventos(ENVIADAS[1:]))

    linhas, aprovado = relatorio(ponte, _banco(["tentativa ainda aberta"]), 650.0)

    assert not aprovado
    assert "  tentativa ainda aberta" in linhas
