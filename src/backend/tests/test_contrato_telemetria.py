import json
from datetime import datetime, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

import pytest
from contrato.telemetria import (
    TAMANHO_MAXIMO,
    Comando,
    Deduplicador,
    EntradaPonte,
    LinhaInvalida,
    RelogioDoRobo,
    RespostaPonte,
    escrever_linha,
    ler_linha,
)

EXEMPLOS = Path(__file__).resolve().parents[2] / "contrato" / "exemplos.jsonl"
LINHAS = EXEMPLOS.read_text(encoding="utf-8").splitlines(keepends=True)

TEL = json.loads(LINHAS[3])


def _linha(**campos) -> str:
    return json.dumps(campos, separators=(",", ":")) + "\n"


def _tel(**mudancas) -> str:
    return _linha(**{**TEL, **mudancas})


def _evento_bruto(**campos) -> str:
    return _linha(v=1, boot=7, seq=1, t_ms=1, **campos)


def _falha(**campos) -> str:
    return _evento_bruto(tipo="falha", x=0, y=0, **campos)


def test_exemplos_cobrem_todos_os_tipos():
    tipos = {json.loads(linha)["tipo"] for linha in LINHAS}
    assert tipos == {"hc_item", "hc_resultado", "tel", "passo", "falha", "sucesso"}


@pytest.mark.parametrize("linha", LINHAS, ids=lambda linha: json.loads(linha)["tipo"])
def test_exemplo_ida_e_volta_byte_a_byte(linha):
    assert len(linha.encode("utf-8")) <= TAMANHO_MAXIMO
    assert escrever_linha(ler_linha(linha)) == linha


@pytest.mark.parametrize(
    "linha",
    [
        "não é json\n",
        _tel(v=2),
        _tel(tipo="heartbeat"),
        _tel(x=12),
        _tel(y=-1),
        _tel(x="3"),
        _tel(rumo="NE"),
        _tel(bat_mv=7810.5),
        _tel(estado="parado"),
        _tel(eixo_longo="z"),
        _linha(**{k: v for k, v in TEL.items() if k != "bat_mv"}),
        _tel(extra="x" * TAMANHO_MAXIMO),
        _evento_bruto(tipo="passo", x=0, y=0, paredes=16),
        _evento_bruto(tipo="hc_item", componente="tof_traseiro", aprovado=True, valor=None),
        _evento_bruto(tipo="hc_item", componente="tof_frontal_esq", aprovado=True, valor=300),
        _evento_bruto(tipo="hc_resultado", aprovado=True, tipo_dip="invalido", inicio="nova"),
        _falha(motivo="collision", origem="web", componente=None),
        _falha(motivo="stuck", origem="boot", componente=None),
        _falha(motivo="time_exceeded", origem="automatica", componente=None),
        _falha(motivo="falha_componente", origem="automatica", componente=None),
        _falha(motivo="collision", origem="automatica", componente="tof_direito"),
    ],
)
def test_linha_invalida_e_descartada(linha):
    with pytest.raises(LinhaInvalida):
        ler_linha(linha)


def test_campo_extra_e_ignorado_sem_mudar_versao():
    mensagem = ler_linha(_tel(temp_c=31))
    assert mensagem.v == 1
    assert escrever_linha(mensagem) == LINHAS[3]


def test_hc_item_do_tof_frontal_e_aceito():
    linha = _evento_bruto(tipo="hc_item", componente="tof_frontal", aprovado=True, valor=300)
    assert ler_linha(linha).componente == "tof_frontal"
    assert escrever_linha(ler_linha(linha)) == linha


def test_aceita_linha_em_bytes_e_com_crlf():
    assert ler_linha(LINHAS[3].rstrip("\n").encode() + b"\r\n").seq == TEL["seq"]


def _evento(boot: int, seq: int):
    return ler_linha(_linha(v=1, boot=boot, seq=seq, t_ms=seq, tipo="passo", x=0, y=0, paredes=0))


def test_deduplicador_descarta_reenvio_e_aceita_novos():
    dedup = Deduplicador()
    assert [dedup.eh_nova(_evento(7, s)) for s in (1, 2, 3)] == [True, True, True]
    # reconexão: o buffer inteiro volta, seguido de um evento novo
    assert [dedup.eh_nova(_evento(7, s)) for s in (1, 2, 3, 4)] == [False, False, False, True]


def test_deduplicador_separa_por_boot_e_deixa_tel_passar():
    dedup = Deduplicador()
    assert dedup.eh_nova(_evento(7, 50))
    assert dedup.eh_nova(_evento(8, 0))  # reinício: seq volta a zero
    tel = ler_linha(LINHAS[3])
    assert dedup.eh_nova(tel) and dedup.eh_nova(tel)


def test_comando_interromper():
    assert Comando().model_dump_json() == '{"v":1,"cmd":"interromper"}'
    resposta = RespostaPonte(comandos=[Comando()])
    assert resposta.model_dump_json() == '{"comandos":[{"v":1,"cmd":"interromper"}]}'
    assert RespostaPonte().model_dump_json() == '{"comandos":[]}'


def test_entrada_da_ponte_exige_fuso_no_recebido_em():
    entrada = EntradaPonte.model_validate_json(
        '{"linha":"{}","recebido_em":"2026-10-05T14:03:21.512-03:00"}'
    )
    assert entrada.recebido_em.utcoffset().total_seconds() == -3 * 3600
    with pytest.raises(ValueError):
        EntradaPonte.model_validate_json('{"linha":"{}","recebido_em":"2026-10-05T14:03:21"}')


def test_relogio_converte_t_ms_em_hora_de_brasilia():
    brasilia = ZoneInfo("America/Sao_Paulo")
    relogio = RelogioDoRobo()
    primeira = ler_linha(LINHAS[0])  # boot 7, t_ms 412
    chegada = datetime(2026, 10, 5, 14, 0, 0, 50_000, tzinfo=brasilia)
    assert relogio.enviado_em(primeira, chegada) == chegada

    # mensagem seguinte chega com mais atraso: o intervalo continua exato
    tel = ler_linha(LINHAS[3])  # boot 7, t_ms 11250
    atrasada = chegada + timedelta(milliseconds=11250 - 412 + 300)
    assert relogio.enviado_em(tel, atrasada) == chegada + timedelta(milliseconds=11250 - 412)

    # outro boot ganha outra âncora
    outra = ler_linha(LINHAS[6])  # boot 8
    assert relogio.enviado_em(outra, atrasada) == atrasada
