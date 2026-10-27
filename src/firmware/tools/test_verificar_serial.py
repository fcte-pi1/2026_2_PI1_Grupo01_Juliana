"""Testes do verificador da serial."""

import json
import sys

import pytest
from verificar_serial import ler_linhas, main


def mensagem(seq: int, tipo: str = "tel", boot: int = 3, t_ms: int | None = None) -> str:
    """Uma linha do contrato v1 com o cabeçalho pedido."""
    corpo = {"v": 1, "boot": boot, "seq": seq, "t_ms": seq * 200 if t_ms is None else t_ms}
    if tipo == "tel":
        corpo |= {"tipo": "tel", "estado": "running", "x": 0, "y": 0, "rumo": "N",
                  "bat_mv": 7800, "vel_mm_s": 0, "eixo_longo": None}
    else:
        corpo |= {"tipo": "passo", "x": 0, "y": seq, "paredes": 9}
    return json.dumps(corpo, separators=(",", ":")) + "\n"


def rodar(tmp_path, linhas: list[str], capsys) -> tuple[int, str]:
    """Grava as linhas num arquivo, roda o verificador e devolve código e saída."""
    caminho = tmp_path / "captura.jsonl"
    caminho.write_text("".join(linhas), encoding="utf-8")
    codigo = main(["--arquivo", str(caminho)])
    return codigo, capsys.readouterr().out


def test_tudo_certo_sai_0(tmp_path, capsys):
    linhas = [mensagem(0), mensagem(1, "passo"), mensagem(2), mensagem(0, boot=4)]
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 0
    assert "tel: 3" in saida
    assert "passo: 1" in saida
    assert "Buracos de seq: 0" in saida
    assert "Linhas inválidas: 0" in saida


def test_seq_pulado_sai_1_e_mostra_o_numero(tmp_path, capsys):
    codigo, saida = rodar(tmp_path, [mensagem(0), mensagem(1), mensagem(3)], capsys)
    assert codigo == 1
    assert "esperado seq 2, recebido seq 3" in saida


def test_linha_de_257_bytes_sai_1(tmp_path, capsys):
    base = mensagem(1).rstrip("\n")
    longa = base[:-1] + ',"x_extra":"' + "a" * (257 - len(base) - 14) + '"}\n'
    assert len(longa.encode()) == 257
    codigo, saida = rodar(tmp_path, [mensagem(0), longa], capsys)
    assert codigo == 1
    assert "linha 2: 257 bytes" in saida


def test_linha_de_256_bytes_passa(tmp_path, capsys):
    base = mensagem(1).rstrip("\n")
    cheia = base[:-1] + ',"x_extra":"' + "a" * (256 - len(base) - 14) + '"}\n'
    assert len(cheia.encode()) == 256
    codigo, _ = rodar(tmp_path, [mensagem(0), cheia], capsys)
    assert codigo == 0


def test_linha_de_depuracao_no_meio_e_ignorada(tmp_path, capsys):
    linhas = [mensagem(0), "# tel desc_fila=0 desc_buffer=0 nav_periodo_max_ms=12\n", mensagem(1)]
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 0
    assert "tel: 2" in saida


@pytest.mark.parametrize(
    ("linha", "motivo"),
    [
        ("{nao e json\n", "JSON inválido"),
        ('{"v":2,"boot":3,"seq":1,"t_ms":0,"tipo":"tel"}\n', "v = 2"),
        ('{"v":1,"boot":3,"seq":1,"t_ms":0,"tipo":"heartbeat"}\n', "tipo desconhecido"),
    ],
)
def test_linha_invalida_sai_1(tmp_path, capsys, linha, motivo):
    codigo, saida = rodar(tmp_path, [mensagem(0), linha], capsys)
    assert codigo == 1
    assert motivo in saida


def test_taxa_da_tel_pelo_t_ms(tmp_path, capsys):
    linhas = [mensagem(seq) for seq in range(51)]  # 50 intervalos de 200 ms
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 0
    assert "Duração (pelo t_ms): 10.0 s" in saida
    assert "Taxa média da tel: 5.10 Hz" in saida


BOOT_DA_ESP32 = [
    "\r\n",
    "rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n",
    "load:0x3fff0030,len:1184\r\n",
    "entry 0x400805e4\r\n",
]


def test_lixo_antes_da_primeira_mensagem_e_descartado(tmp_path, capsys):
    repetido = ['xo_longo":null}\n'] * 3
    pedaco = '{"v":1,"boot":1,"seq":30,"t_ms":{"v":1,\n'
    linhas = BOOT_DA_ESP32 + repetido + [pedaco, mensagem(0), mensagem(1)]
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 0
    assert "Descartadas antes da primeira mensagem: 7" in saida
    assert "Linhas inválidas: 0" in saida
    assert "tel: 2" in saida


def test_lixo_depois_da_primeira_mensagem_continua_invalido(tmp_path, capsys):
    codigo, saida = rodar(tmp_path, [mensagem(0), 'xo_longo":null}\n', mensagem(1)], capsys)
    assert codigo == 1
    assert "linha 2: JSON inválido" in saida


def test_depuracao_antes_da_primeira_mensagem_nao_conta_como_descartada(tmp_path, capsys):
    linhas = ["# Ratatouille: firmware com HAL SIMULADA\n", mensagem(0)]
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 0
    assert "Descartadas antes da primeira mensagem: 0" in saida


@pytest.mark.parametrize("linhas", [[], BOOT_DA_ESP32, ["# so depuracao\n"]])
def test_captura_sem_mensagem_do_contrato_sai_1(tmp_path, capsys, linhas):
    codigo, saida = rodar(tmp_path, linhas, capsys)
    assert codigo == 1
    assert "Nenhuma mensagem do contrato recebida" in saida
    assert "Resultado: FALHOU" in saida


def test_modo_arquivo_nao_importa_pyserial(tmp_path, capsys, monkeypatch):
    monkeypatch.setitem(sys.modules, "serial", None)  # import serial falharia
    codigo, _ = rodar(tmp_path, [mensagem(0)], capsys)
    assert codigo == 0


class ConexaoFalsa:
    """Imita a porta serial: devolve os pedaços pedidos, um por readline."""

    def __init__(self, pedacos: list[bytes]):
        self.pedacos = list(pedacos)
        self.entrada_limpa = False

    def reset_input_buffer(self) -> None:
        self.entrada_limpa = True

    def readline(self) -> bytes:
        return self.pedacos.pop(0) if self.pedacos else b""


def test_porta_aberta_no_meio_descarta_o_pedaco_da_primeira_linha():
    inteira = mensagem(0).encode()
    conexao = ConexaoFalsa([b'"x":0}\n', inteira, b"", mensagem(1).encode()])
    linhas = []
    for linha in ler_linhas(conexao, 1.0):
        linhas.append(linha)
        if len(linhas) == 2:
            break
    assert conexao.entrada_limpa
    assert linhas == [inteira, mensagem(1).encode()]


def test_linha_partida_pelo_timeout_do_readline_sai_inteira():
    inteira = mensagem(0).encode()
    conexao = ConexaoFalsa([b'"x":0}\n', inteira[:20], b"", inteira[20:]])
    linhas = []
    for linha in ler_linhas(conexao, 1.0):
        linhas.append(linha)
        break
    assert linhas == [inteira]
