"""Caso 4.3 do 7.4: encerramento manual e interrupção pendente (RF15, RF30, RF39).

Liga a API real (banco de teste) à ponte real por um pty, que faz o papel do robô.
O `interromper` só sai na resposta ao próximo POST /telemetria da ponte, então o
tempo até a serial depende da cadência da `tel`.
"""

import json
import os
import select
import sys
import threading
import time
import uuid
from datetime import UTC, datetime
from statistics import median
from typing import NamedTuple

import pytest
from app.db import get_db
from app.main import app
from app.models import (
    ExecucaoLogica,
    Falha,
    HealthCheck,
    HealthCheckItem,
    Labirinto,
    LeituraTelemetria,
    ParedeCelula,
    PassoTrajeto,
    Tentativa,
    TentativaRejeitada,
)
from fastapi.testclient import TestClient
from ponte.__main__ import Ponte
from ponte.config import ConfigPonte
from ponte.leitora import abrir_serial
from sqlalchemy import delete, func
from sqlalchemy import select as consulta
from sqlalchemy.orm import sessionmaker

pytestmark = pytest.mark.skipif(
    not sys.platform.startswith(("linux", "darwin")), reason="pty só existe em Linux e macOS"
)

TEL = {
    "v": 1,
    "boot": 7,
    "tipo": "tel",
    "estado": "running",
    "x": 0,
    "y": 0,
    "rumo": "N",
    "bat_mv": 7810,
    "vel_mm_s": 210,
    "eixo_longo": None,
}


def _linha(seq: int, **campos) -> str:
    return json.dumps({**TEL, "seq": seq, "t_ms": 200 * seq, **campos}, separators=(",", ":"))


def ler_do_mestre(mestre, esperado, limite):
    """Lê o lado mestre até receber `esperado` ou estourar `limite` segundos."""
    lido = b""
    fim = time.monotonic() + limite
    while esperado not in lido:
        restante = fim - time.monotonic()
        if restante <= 0:
            break
        prontos, _, _ = select.select([mestre], [], [], restante)
        if prontos:
            lido += os.read(mestre, 1024)
    return lido


def abrir_avisando(aberta):
    """Abre a porta de verdade e sinaliza `aberta`."""

    def abrir(caminho):
        porta = abrir_serial(caminho)
        aberta.set()
        return porta

    return abrir


class Aberta(NamedTuple):
    execucao_id: uuid.UUID
    tentativa_id: uuid.UUID


class Montagem:
    """API real, ponte real e o lado mestre do pty no papel do robô."""

    def __init__(self, mestre, ponte, cliente, sessoes) -> None:
        self.mestre = mestre
        self.ponte = ponte
        self.cliente = cliente
        self.sessoes = sessoes
        self.execucoes: list[uuid.UUID] = []
        self.health_checks: list[uuid.UUID] = []

    def abrir_tentativa(self, tipo: str = "4x4") -> Aberta:
        """Execução em andamento com uma tentativa running, gravadas com commit."""
        with self.sessoes() as sessao:
            agora = datetime.now(UTC)
            execucao = ExecucaoLogica(
                execucao_id=uuid.uuid4(),
                labirinto_id=sessao.scalar(
                    consulta(Labirinto.labirinto_id).where(Labirinto.tipo == tipo)
                ),
                status="em_andamento",
                tentativas_usadas=1,
                iniciada_em=agora,
            )
            sessao.add(execucao)
            self.execucoes.append(execucao.execucao_id)
            sessao.flush()
            tentativa = Tentativa(
                tentativa_id=uuid.uuid4(),
                execucao_id=execucao.execucao_id,
                attempt_index=1,
                status="running",
                tipo_inicio="nova",
                iniciada_em=agora,
            )
            sessao.add(tentativa)
            sessao.commit()
            return Aberta(execucao.execucao_id, tentativa.tentativa_id)

    def escrever(self, linha: str) -> None:
        os.write(self.mestre, linha.encode() + b"\n")

    def contar_leituras(self, tentativa_id: uuid.UUID) -> int:
        with self.sessoes() as sessao:
            return sessao.scalar(
                consulta(func.count())
                .select_from(LeituraTelemetria)
                .where(LeituraTelemetria.tentativa_id == tentativa_id)
            )

    def esperar_leituras(self, tentativa_id: uuid.UUID, quantidade: int, limite=2.0) -> int:
        fim = time.monotonic() + limite
        contadas = self.contar_leituras(tentativa_id)
        while contadas < quantidade and time.monotonic() < fim:
            time.sleep(0.02)
            contadas = self.contar_leituras(tentativa_id)
        return contadas

    def apagar_o_que_criou(self) -> None:
        """Deixa o banco de teste como estava antes do teste."""
        with self.sessoes() as sessao:
            tentativas = consulta(Tentativa.tentativa_id).where(
                Tentativa.execucao_id.in_(self.execucoes)
            )
            for modelo in (LeituraTelemetria, PassoTrajeto, Falha):
                sessao.execute(delete(modelo).where(modelo.tentativa_id.in_(tentativas)))
            for modelo in (ParedeCelula, TentativaRejeitada, Tentativa):
                sessao.execute(delete(modelo).where(modelo.execucao_id.in_(self.execucoes)))
            sessao.execute(
                delete(ExecucaoLogica).where(ExecucaoLogica.execucao_id.in_(self.execucoes))
            )
            sessao.execute(
                delete(HealthCheckItem).where(HealthCheckItem.hc_id.in_(self.health_checks))
            )
            sessao.execute(delete(HealthCheck).where(HealthCheck.hc_id.in_(self.health_checks)))
            sessao.commit()


@pytest.fixture
def montagem(engine, tmp_path):
    """Sessões próprias (uma por requisição): a ponte posta numa thread e o teste em outra."""
    sessoes = sessionmaker(bind=engine, autocommit=False, autoflush=False)

    def sessao_por_requisicao():
        sessao = sessoes()
        try:
            yield sessao
        finally:
            sessao.close()

    app.dependency_overrides[get_db] = sessao_por_requisicao
    mestre, escravo = os.openpty()
    caminho = os.ttyname(escravo)
    # cada entrada no TestClient roda o lifespan: GerenciadorExecucoes novo, sem pendências
    with TestClient(app) as cliente:
        aberta = threading.Event()
        config = ConfigPonte(_env_file=None, porta_serial=caminho, log_dir=tmp_path)
        ponte = Ponte(config, cliente=cliente, abrir=abrir_avisando(aberta))
        montagem = Montagem(mestre, ponte, cliente, sessoes)
        ponte.iniciar()
        try:
            assert aberta.wait(2), "a ponte não abriu o pty"
            # a leitora descarta a primeira linha depois de abrir, que pode vir pela metade
            os.write(mestre, b"\n")
            yield montagem
        finally:
            ponte.encerrar()
            montagem.apagar_o_que_criou()
    app.dependency_overrides.clear()
    os.close(mestre)
    os.close(escravo)


def test_tel_valida_chega_ao_banco_sem_comando_na_serial(montagem):
    tentativa = montagem.abrir_tentativa()

    montagem.escrever(_linha(1))

    assert montagem.esperar_leituras(tentativa.tentativa_id, 1) == 1, "a tel não foi gravada"
    assert ler_do_mestre(montagem.mestre, b"\n", limite=0.3) == b"", "chegou comando na serial"
    gerenciador = app.state.gerenciador
    assert gerenciador.comandos_pendentes() == [], "o gerenciador tem comando pendente"


INTERROMPER = b'{"v":1,"cmd":"interromper"}\n'
REPETICOES = 20


class Robo:
    """Thread no papel do robô: escreve uma `tel` running no mestre a cada `periodo`."""

    def __init__(self, montagem: Montagem, periodo: float) -> None:
        self.montagem = montagem
        self.periodo = periodo
        self.escritas = 0
        self.ultima = 0.0
        self._mudou = threading.Condition()
        self._parar = threading.Event()
        self._thread = threading.Thread(target=self._rodar, daemon=True)

    def __enter__(self) -> "Robo":
        self._thread.start()
        with self._mudou:
            self._mudou.wait_for(lambda: self.escritas > 0, timeout=2)
        return self

    def __exit__(self, *_) -> None:
        self._parar.set()
        self._thread.join(2)

    def _rodar(self) -> None:
        proxima = time.monotonic()
        seq = 0
        while not self._parar.is_set():
            seq += 1
            self.montagem.escrever(_linha(seq))
            with self._mudou:
                self.ultima = time.monotonic()
                self.escritas += 1
                self._mudou.notify_all()
            proxima += self.periodo
            self._parar.wait(max(0.0, proxima - time.monotonic()))

    def esperar_fase(self, fase: float) -> None:
        """Volta `fase` segundos depois da última `tel` escrita."""
        with self._mudou:
            if time.monotonic() >= self.ultima + fase:
                vistas = self.escritas
                self._mudou.wait_for(lambda: self.escritas > vistas, timeout=2 * self.periodo)
            alvo = self.ultima + fase
        time.sleep(max(0.0, alvo - time.monotonic()))


def medir_encerramentos(montagem: Montagem, periodo: float) -> tuple[list[float], list[Aberta]]:
    """Encerra 20 tentativas novas, a 0%, 5%, ..., 95% do período depois da última `tel`."""
    gerenciador = app.state.gerenciador
    medidas = []
    abertas = []
    with Robo(montagem, periodo) as robo:
        for k in range(REPETICOES):
            aberta = montagem.abrir_tentativa()
            abertas.append(aberta)
            robo.esperar_fase(k * 0.05 * periodo)
            inicio = time.monotonic()
            resposta = montagem.cliente.post(
                f"/execucoes/{aberta.execucao_id}/encerrar-tentativa",
                json={"motivo": "collision"},
            )
            assert resposta.status_code == 200, resposta.text
            lido = ler_do_mestre(montagem.mestre, INTERROMPER, limite=3 * periodo)
            medidas.append(time.monotonic() - inicio)
            assert INTERROMPER in lido, f"o interromper não chegou na repetição {k}: {lido!r}"
            # o robô confirmaria com a falha web; aqui a confirmação limpa a pendência
            gerenciador._confirmar_interrupcao(None)
    return medidas, abertas


def conferir_failed_encerrado_operador(montagem: Montagem, abertas: list[Aberta]) -> None:
    with montagem.sessoes() as sessao:
        for aberta in abertas:
            tentativa = sessao.get(Tentativa, aberta.tentativa_id)
            assert tentativa.status == "failed", f"tentativa {aberta.tentativa_id} não falhou"
            assert tentativa.falha.origem == "encerrado_operador", "a falha não veio do operador"


def resumir(rotulo: str, medidas: list[float]) -> str:
    return (
        f"{rotulo}: {len(medidas)} encerramentos, mediana {median(medidas) * 1000:.0f} ms, "
        f"maximo {max(medidas) * 1000:.0f} ms"
    )


# a 5 Hz vale o critério de 1 s; a 1 Hz (robô parado) é observação, com limite só de sanidade
@pytest.mark.parametrize(
    ("rotulo", "periodo", "limite"),
    [("5 Hz", 0.2, 1.0), ("1 Hz", 1.0, 2.0)],
    ids=["5_hz", "1_hz"],
)
def test_encerrar_tentativa_chega_a_serial_dentro_do_limite(montagem, rotulo, periodo, limite):
    medidas, abertas = medir_encerramentos(montagem, periodo)

    print("\n" + resumir(rotulo, medidas))
    assert len(medidas) == REPETICOES
    conferir_failed_encerrado_operador(montagem, abertas)
    assert max(medidas) < limite, f"o interromper passou de {limite} s: {max(medidas):.3f} s"


def _falha_web(seq: int) -> str:
    """Confirmação da interrupção pelo robô, válida pelo contrato v1."""
    falha = {
        "v": 1,
        "boot": TEL["boot"],
        "seq": seq,
        "t_ms": 200 * seq,
        "tipo": "falha",
        "motivo": None,
        "origem": "web",
        "x": 0,
        "y": 0,
        "componente": None,
    }
    return json.dumps(falha, separators=(",", ":"))


def respostas_da_api(montagem: Montagem) -> list[str]:
    """Linhas `API <status> comandos=<n>` da auditoria, uma por POST respondido."""
    texto = montagem.ponte.auditoria.caminho.read_text(encoding="utf-8")
    return [linha for linha in texto.splitlines() if " API " in linha and "comandos=" in linha]


def enviar_e_ler(montagem: Montagem, linha: str, limite=2.0) -> bytes:
    """Escreve `linha` no mestre, espera a ponte receber a resposta e devolve o que voltou."""
    antes = len(respostas_da_api(montagem))
    montagem.escrever(linha)
    fim = time.monotonic() + limite
    while len(respostas_da_api(montagem)) <= antes:
        assert time.monotonic() < fim, f"a API não respondeu à linha {linha}"
        time.sleep(0.01)
    if respostas_da_api(montagem)[antes].endswith("comandos=0"):
        return ler_do_mestre(montagem.mestre, b"\n", limite=0.05)
    return ler_do_mestre(montagem.mestre, INTERROMPER, limite)


def conferir_pendencia_ate_a_falha_web(montagem: Montagem) -> None:
    """O interromper vai em toda resposta até a falha web e para depois dela."""
    for seq in (2, 3):
        lido = enviar_e_ler(montagem, _linha(seq))
        assert lido == INTERROMPER, f"a tel {seq} não trouxe o interromper: {lido!r}"
    lido = enviar_e_ler(montagem, _falha_web(4))
    assert lido == b"", f"a confirmação ainda trouxe comando: {lido!r}"
    for seq in (5, 6):
        lido = enviar_e_ler(montagem, _linha(seq))
        assert lido == b"", f"a tel {seq} trouxe comando depois da confirmação: {lido!r}"
    auditoria = montagem.ponte.auditoria.caminho.read_text(encoding="utf-8")
    assert 'TX {"v":1,"cmd":"interromper"}' in auditoria, "a auditoria não registrou o TX"


def test_encerrar_tentativa_repete_o_interromper_ate_a_falha_web(montagem):
    aberta = montagem.abrir_tentativa()
    assert enviar_e_ler(montagem, _linha(1)) == b"", "chegou comando antes do encerramento"

    resposta = montagem.cliente.post(
        f"/execucoes/{aberta.execucao_id}/encerrar-tentativa", json={"motivo": "collision"}
    )

    assert resposta.status_code == 200, resposta.text
    conferir_pendencia_ate_a_falha_web(montagem)
    conferir_failed_encerrado_operador(montagem, [aberta])
    with montagem.sessoes() as sessao:
        execucao = sessao.get(ExecucaoLogica, aberta.execucao_id)
        assert execucao.status == "em_andamento", "a execução não ficou aberta para retomada"


def test_encerrar_execucao_cancela_e_repete_o_interromper_ate_a_falha_web(montagem):
    aberta = montagem.abrir_tentativa()
    assert enviar_e_ler(montagem, _linha(1)) == b"", "chegou comando antes do encerramento"

    resposta = montagem.cliente.post(f"/execucoes/{aberta.execucao_id}/encerrar")

    assert resposta.status_code == 200, resposta.text
    conferir_pendencia_ate_a_falha_web(montagem)
    conferir_failed_encerrado_operador(montagem, [aberta])
    with montagem.sessoes() as sessao:
        execucao = sessao.get(ExecucaoLogica, aberta.execucao_id)
        assert execucao.status == "cancelada", "a execução não foi cancelada"
