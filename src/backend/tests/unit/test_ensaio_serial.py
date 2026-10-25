from datetime import datetime, timedelta, timezone

import pytest
from scripts.ensaio_serial import EventoPonte, ler_log_ponte, ler_log_simulador

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
