import threading
from datetime import datetime, timedelta, timezone

from ponte.auditoria import Auditoria

FUSO = timezone(timedelta(hours=-3))
AGORA = datetime(2026, 10, 21, 9, 5, 7, 123456, tzinfo=FUSO)


def relogio():
    return AGORA


def test_nome_do_arquivo(tmp_path):
    with Auditoria(tmp_path, relogio=relogio) as auditoria:
        assert auditoria.caminho == tmp_path / "ponte-20261021-090507.log"
    assert auditoria.caminho.exists()


def test_cria_o_diretorio(tmp_path):
    log_dir = tmp_path / "logs" / "ponte"
    with Auditoria(log_dir, relogio=relogio) as auditoria:
        assert auditoria.caminho.parent == log_dir
    assert log_dir.is_dir()


def test_formato_da_linha_com_flush(tmp_path):
    with Auditoria(tmp_path, relogio=relogio) as auditoria:
        auditoria.registrar("RX", '{"v":1,"tipo":"tel"}')
        auditoria.registrar("CONEXAO", "aberta /dev/rfcomm0")
        linhas = auditoria.caminho.read_text(encoding="utf-8").splitlines()
    assert linhas == [
        '2026-10-21T09:05:07.123-03:00 RX {"v":1,"tipo":"tel"}',
        "2026-10-21T09:05:07.123-03:00 CONEXAO aberta /dev/rfcomm0",
    ]


def test_duas_threads_sem_misturar_linhas(tmp_path):
    total = 500
    with Auditoria(tmp_path, relogio=relogio) as auditoria:

        def escrever(tipo):
            for i in range(total):
                auditoria.registrar(tipo, f"{tipo.lower()}-{i:04d} " + "x" * 200)

        threads = [threading.Thread(target=escrever, args=(t,)) for t in ("RX", "TX")]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
    linhas = auditoria.caminho.read_text(encoding="utf-8").splitlines()
    assert len(linhas) == 2 * total
    for tipo in ("RX", "TX"):
        esperadas = [
            f"2026-10-21T09:05:07.123-03:00 {tipo} {tipo.lower()}-{i:04d} " + "x" * 200
            for i in range(total)
        ]
        assert [linha for linha in linhas if f" {tipo} " in linha] == esperadas
