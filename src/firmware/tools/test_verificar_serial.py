"""Testes do verificador da serial."""

import json
import sys

import pytest
from verificar_serial import SEM_ENLACE, ler_linhas, ler_serial, main, verificar


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
        self.escritas: list[bytes] = []

    def reset_input_buffer(self) -> None:
        self.entrada_limpa = True

    def readline(self) -> bytes:
        return self.pedacos.pop(0) if self.pedacos else b""

    def write(self, dados: bytes) -> None:
        self.escritas.append(dados)


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


def test_sem_largar_nada_e_escrito_na_porta():
    conexao = ConexaoFalsa([b'"x":0}\n', mensagem(0).encode()])
    for _ in ler_linhas(conexao, 0.3):
        pass
    assert conexao.escritas == []


def test_largar_escreve_o_comando_uma_vez_depois_do_prazo():
    conexao = ConexaoFalsa([b'"x":0}\n', mensagem(0).encode(), b"", b"", mensagem(1).encode()])
    for _ in ler_linhas(conexao, 0.3, largar_apos_s=0.0):
        pass
    assert conexao.escritas == [b'{"v":1,"cmd":"largar"}\n']


def test_largar_antes_do_prazo_nao_escreve():
    conexao = ConexaoFalsa([b'"x":0}\n', mensagem(0).encode()])
    for _ in ler_linhas(conexao, 0.2, largar_apos_s=60.0):
        pass
    assert conexao.escritas == []


def _com_chegadas(atrasos_s: list[float]) -> str:
    """Resumo de uma captura em que a mensagem i (a cada 1 s do robô) chega com o atraso dado."""
    linhas = [mensagem(i, t_ms=i * 1000).encode() for i in range(len(atrasos_s))]
    horas = iter(i + atraso for i, atraso in enumerate(atrasos_s))
    verificacao = verificar(linhas, relogio=lambda: next(horas) + 100.0)
    return verificacao


def test_atraso_de_entrega_baixo_passa():
    verificacao = _com_chegadas([0.2, 0.25, 0.2, 0.3, 0.2, 0.22, 0.21, 0.2, 0.23, 0.2])
    assert verificacao.atraso_p95_s() is not None
    assert verificacao.atraso_p95_s() < 0.2
    assert verificacao.ok()
    assert "Atraso de entrega" in verificacao.resumo()


def test_atraso_de_entrega_alto_falha_pelo_p95():
    # 8 de 20 mensagens chegam 4 s depois do melhor caso: o p95 passa de 3 s
    verificacao = _com_chegadas([0.2] * 12 + [4.2] * 8)
    assert verificacao.atraso_p95_s() > 3.0
    assert not verificacao.ok()
    assert "Resultado: FALHOU" in verificacao.resumo()


def test_modo_arquivo_nao_tem_atraso_de_entrega(tmp_path, capsys):
    codigo, saida = rodar(tmp_path, [mensagem(0), mensagem(1)], capsys)
    assert codigo == 0
    assert "Atraso de entrega" not in saida


class _Conexao:
    """Conexão de mentira: entrega as linhas dadas e, no fim, cai (se `cai`) ou devolve vazio."""

    def __init__(self, linhas: list[bytes], cai: bool):
        self.linhas = list(linhas)
        self.cai = cai

    def reset_input_buffer(self):
        pass

    def readline(self):
        if self.linhas:
            return self.linhas.pop(0)
        if self.cai:
            raise OSError("device reports readiness to read but returned no data")
        return b""

    def write(self, dados):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False


def test_queda_do_enlace_vira_none_e_a_porta_reabre():
    conexoes = iter(
        [
            _Conexao([b"lixo\n", mensagem(0).encode()], cai=True),
            _Conexao([b"lixo\n", mensagem(1).encode()], cai=False),
        ]
    )
    esperas: list[float] = []
    # duracao curta: a segunda conexao devolve vazio ate o prazo acabar
    entregues = list(
        ler_serial("/dev/rfcomm0", 115200, 0.2, abrir=lambda p, b: next(conexoes), dormir=esperas.append)
    )
    assert entregues.count(None) == 1
    assert esperas and esperas[0] == 0.5


def test_queda_conta_no_resumo_e_falha():
    verificacao = verificar([mensagem(0).encode(), None, mensagem(1).encode()])
    assert verificacao.quedas == 1
    assert "Quedas do enlace: 1" in verificacao.resumo()
    assert not verificacao.ok()


def test_sem_queda_mostra_zero(tmp_path, capsys):
    codigo, saida = rodar(tmp_path, [mensagem(0), mensagem(1)], capsys)
    assert codigo == 0
    assert "Quedas do enlace: 0" in saida


def test_verificar_preenche_a_verificacao_dada():
    from verificar_serial import Verificacao

    dada = Verificacao()
    resultado = verificar([mensagem(0).encode(), mensagem(1).encode()], verificacao=dada)
    assert resultado is dada
    assert dada.por_tipo["tel"] == 2


def test_health_check_escreve_o_comando_dado_uma_vez():
    from verificar_serial import COMANDO_HEALTH_CHECK

    escritas: list[bytes] = []

    class Conexao(_Conexao):
        def write(self, dados):
            escritas.append(dados)

    conexao = Conexao([mensagem(0).encode()], cai=False)
    list(ler_linhas(conexao, 0.3, 0.0, COMANDO_HEALTH_CHECK))
    assert escritas == [COMANDO_HEALTH_CHECK]


def test_conexao_que_nunca_entrega_nada_e_sem_enlace_e_nao_queda():
    conexoes = iter([_Conexao([], cai=True), _Conexao([], cai=True), _Conexao([], cai=False)])
    entregues = list(
        ler_serial("/dev/rfcomm0", 115200, 0.2, abrir=lambda p, b: next(conexoes), dormir=lambda s: None)
    )
    assert entregues.count(SEM_ENLACE) == 2
    assert entregues.count(None) == 0
    verificacao = verificar(entregues)
    assert verificacao.quedas == 0
    assert verificacao.sem_enlace == 2
    resumo = verificacao.resumo()
    assert "Tentativas de conexão sem nenhuma mensagem: 2" in resumo
    assert "O enlace não abriu" in resumo


def test_rfcomm_nao_mexe_no_dtr_e_o_usb_baixa(monkeypatch):
    import types

    abertas = []

    class FalsaSerial:
        def __init__(self):
            self.dtr = None
            self.rts = None
            abertas.append(self)

        def open(self):
            pass

    monkeypatch.setitem(sys.modules, "serial", types.SimpleNamespace(Serial=FalsaSerial))
    from verificar_serial import _abrir_serial

    _abrir_serial("/dev/rfcomm0", 115200)
    _abrir_serial("/dev/ttyACM0", 115200)
    assert abertas[0].dtr is None and abertas[0].rts is None
    assert abertas[1].dtr is False and abertas[1].rts is False


def test_eventos_guardados_antes_da_conexao_ficam_fora_do_atraso():
    # tel seq 238 chega agora (t_ms 238000); depois chegam dois hc_item gerados bem antes
    antigo = lambda seq, t: json.dumps(  # noqa: E731
        {"v": 1, "boot": 3, "seq": seq, "t_ms": t, "tipo": "hc_item", "componente": "bateria",
         "aprovado": True, "valor": 7400}, separators=(",", ":")) + "\n"
    linhas = [mensagem(238, t_ms=238000), antigo(1, 1000), antigo(2, 1050), mensagem(239, t_ms=239000)]
    horas = iter([500.0, 500.0, 500.1, 501.0])
    verificacao = verificar([x.encode() for x in linhas], relogio=lambda: next(horas))
    assert verificacao.guardadas_antes_da_conexao() == 2
    assert verificacao.atraso_p95_s() < 0.5
    assert "guardadas pelo robô antes de a conexão abrir" in verificacao.resumo()


def test_maior_silencio_entre_mensagens():
    horas = iter([10.0, 11.0, 31.0, 32.0])
    verificacao = verificar(
        [mensagem(i, t_ms=i * 1000).encode() for i in range(4)], relogio=lambda: next(horas)
    )
    assert verificacao.maior_silencio_s() == 20.0
    assert "Maior silêncio entre mensagens: 20.0 s" in verificacao.resumo()


def _evento(seq: int, t_ms: int) -> bytes:
    return (json.dumps({"v": 1, "boot": 3, "seq": seq, "t_ms": t_ms, "tipo": "hc_item", "componente": "bateria",
                        "aprovado": True, "valor": 7400}, separators=(",", ":")) + "\n").encode()


def test_entrar_no_meio_do_boot_nao_e_buraco():
    # eventos reenviados (seq 1 e 2, gerados no boot) e depois a tel ao vivo no seq 68
    linhas = [_evento(1, 1000), _evento(2, 1050), mensagem(68, t_ms=68000).encode(), mensagem(69, t_ms=69000).encode()]
    relogio = iter([10.0, 10.0, 10.0, 11.0])
    verificacao = verificar(linhas, relogio=lambda: next(relogio))
    assert verificacao.ok()
    assert "Buracos de seq: 0" in verificacao.resumo()


def test_tel_que_some_ao_vivo_continua_sendo_buraco():
    linhas = [mensagem(68, t_ms=68000).encode(), mensagem(70, t_ms=70000).encode()]
    relogio = iter([10.0, 12.0])
    verificacao = verificar(linhas, relogio=lambda: next(relogio))
    assert not verificacao.ok()
    assert verificacao.buracos == [(3, 69, 70)]


def test_no_bluetooth_a_primeira_linha_nao_e_descartada():
    conexao = _Conexao([mensagem(1).encode(), mensagem(2).encode()], cai=False)
    linhas = list(ler_linhas(conexao, 0.3, limpar_inicio=False))
    assert linhas[0] == mensagem(1).encode()


def test_no_usb_a_primeira_linha_continua_sendo_descartada():
    conexao = _Conexao([b"pedaco\n", mensagem(1).encode()], cai=False)
    linhas = list(ler_linhas(conexao, 0.3))
    assert linhas == [mensagem(1).encode()]


def test_duracao_e_taxa_so_contam_o_que_chegou_ao_vivo():
    # dois eventos reenviados de 1000 s antes, depois 10 tel ao vivo a 1 Hz
    linhas = [_evento(1, 1000), _evento(2, 1050)]
    linhas += [mensagem(500 + i, t_ms=500000 + i * 1000).encode() for i in range(10)]
    # a primeira tel chega primeiro; os eventos antigos chegam depois dela
    linhas = [linhas[2], linhas[0], linhas[1]] + linhas[3:]
    relogio = iter(float(i) for i in range(len(linhas)))
    verificacao = verificar(linhas, relogio=lambda: next(relogio))
    assert verificacao.duracao_s() == 9.0
    assert abs(verificacao.taxa_tel_hz() - 10 / 9.0) < 1e-9
