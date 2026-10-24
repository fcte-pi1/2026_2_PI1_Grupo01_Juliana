"""Valida as mudanças novas do firmware até o frontend.

Na API em execução, envia linhas no contrato serial (como a ponte faz com o
robô) e confere o que a tela consome:

- o tipo escolhido (4x4, 8x4, 12x4) permanece no detalhe;
- um passo além do canto 4x4 grava `eixo_longo`, que orienta o mapa;
- encerrar a execução cancela a rodada, registra a falha do operador e faz a
  próxima telemetria devolver `interromper` até a confirmação do firmware;
- encerrar depois de uma tentativa falha impede a retomada;
- uma execução já concluída recusa o encerramento.

Em seguida roda os testes do frontend que cobrem o botão e o desenho do mapa.

Uso, a partir de src/backend, com a API no ar:

    python -m scripts.validar_firmware_frontend
    python -m scripts.validar_firmware_frontend --url http://127.0.0.1:8000
    python -m scripts.validar_firmware_frontend --sem-frontend
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from datetime import UTC, datetime
from pathlib import Path

import httpx

COMPONENTES = (
    "bateria",
    "tof_frontal",
    "tof_esquerdo",
    "tof_direito",
    "motor_esquerdo",
    "motor_direito",
    "encoder_esquerdo",
    "encoder_direito",
)
FRONTEND = Path(__file__).resolve().parents[2] / "frontend"
INTERROMPER = [{"v": 1, "cmd": "interromper"}]


class FalhaValidacao(Exception):
    pass


def _linha(mensagem: dict) -> str:
    return json.dumps(mensagem, separators=(",", ":"))


def _iso_agora() -> str:
    return datetime.now(UTC).isoformat().replace("+00:00", "Z")


class Fluxo:
    def __init__(self, cliente: httpx.Client, boot: int) -> None:
        self.cliente = cliente
        self.boot = boot
        self.seq = 0
        self.t_ms = 0
        self.execucao_id = ""

    def _enviar(self, mensagem: dict) -> httpx.Response:
        self.seq += 1
        self.t_ms += 100
        mensagem = {
            "v": 1,
            "boot": self.boot,
            "seq": self.seq,
            "t_ms": self.t_ms,
            **mensagem,
        }
        resposta = self.cliente.post(
            "/telemetria",
            json={"linha": _linha(mensagem), "recebido_em": _iso_agora()},
        )
        if resposta.status_code != 200:
            tipo = mensagem["tipo"]
            raise FalhaValidacao(f"telemetria {tipo}: {resposta.status_code} {resposta.text}")
        return resposta

    def criar(self, labirinto: str) -> dict:
        resposta = self.cliente.post("/execucoes", json={"tipo_labirinto": labirinto})
        if resposta.status_code != 201:
            raise FalhaValidacao(f"criar {labirinto}: {resposta.status_code} {resposta.text}")
        criada = resposta.json()
        self.execucao_id = criada["execucao_id"]
        return criada

    def detalhe(self) -> dict:
        resposta = self.cliente.get(f"/execucoes/{self.execucao_id}")
        if resposta.status_code != 200:
            raise FalhaValidacao(f"detalhe: {resposta.status_code} {resposta.text}")
        return resposta.json()

    def health_check(self, labirinto: str) -> None:
        for componente in COMPONENTES:
            valor = 7840 if componente == "bateria" else 120
            self._enviar(
                {
                    "tipo": "hc_item",
                    "componente": componente,
                    "aprovado": True,
                    "valor": valor,
                }
            )
        self._enviar(
            {"tipo": "hc_resultado", "aprovado": True, "tipo_dip": labirinto, "inicio": "nova"}
        )

    def passo(self, x: int, y: int, *, eixo_longo: str | None = None) -> None:
        self._enviar({"tipo": "passo", "x": x, "y": y, "paredes": 0})
        self._enviar(
            {
                "tipo": "tel",
                "estado": "running",
                "x": x,
                "y": y,
                "rumo": "N",
                "bat_mv": 7800,
                "vel_mm_s": 180,
                "eixo_longo": eixo_longo,
            }
        )


def _exigir(condicao: bool, mensagem: str) -> None:
    if not condicao:
        raise FalhaValidacao(mensagem)


def _tentativa(detalhe: dict) -> dict:
    tentativas = detalhe.get("tentativas") or []
    _exigir(bool(tentativas), "detalhe sem tentativa")
    return max(tentativas, key=lambda item: item["attempt_index"])


def _cenario_labirinto_e_encerrar(fluxo: Fluxo) -> None:
    """12x4: o firmware revela o eixo e o operador encerra a execução inteira."""
    fluxo.health_check("12x4")
    fluxo.criar("12x4")
    fluxo.passo(0, 0)
    no_canto = fluxo.detalhe()
    _exigir(no_canto["tipo_labirinto"] == "12x4", "o detalhe não guardou o labirinto 12x4")
    _exigir(no_canto["status"] == "em_andamento", "a execução não ficou em andamento")
    _exigir(_tentativa(no_canto)["status"] == "running", "o health-check não passou a running")
    _exigir(no_canto["eixo_longo"] is None, "o canto 4x4 não deveria fixar o eixo longo")
    _exigir(len(no_canto["trajeto"]) == 1, "o primeiro passo do firmware não foi gravado")

    fluxo.passo(4, 0, eixo_longo="x")
    orientada = fluxo.detalhe()
    _exigir(orientada["eixo_longo"] == "x", "o passo x=4 não orientou o mapa em x")
    _exigir(len(orientada["trajeto"]) == 2, "o segundo passo não entrou no trajeto")

    encerrada = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/encerrar")
    _exigir(encerrada.status_code == 200, f"encerrar: {encerrada.status_code} {encerrada.text}")
    corpo = encerrada.json()
    falha = _tentativa(corpo)["falha"]
    _exigir(corpo["status"] == "cancelada", "encerrar não cancelou a execução")
    _exigir(_tentativa(corpo)["status"] == "failed", "a tentativa aberta não foi encerrada")
    _exigir(
        falha["motivo"] == "encerrado_operador" and falha["origem"] == "encerrado_operador",
        f"falha inesperada: {falha}",
    )

    confirmacao = fluxo._enviar(
        {
            "tipo": "tel",
            "estado": "running",
            "x": 4,
            "y": 0,
            "rumo": "N",
            "bat_mv": 7790,
            "vel_mm_s": 0,
            "eixo_longo": "x",
        }
    )
    _exigir(
        confirmacao.json().get("comandos") == INTERROMPER,
        "a ponte não recebeu o comando interromper",
    )
    parado = fluxo._enviar(
        {
            "tipo": "falha",
            "motivo": None,
            "origem": "web",
            "x": 4,
            "y": 0,
            "componente": None,
        }
    )
    _exigir(
        parado.json().get("comandos") in (None, []),
        "a confirmação do firmware não encerrou a interrupção",
    )

    retomada = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/retomar")
    _exigir(retomada.status_code == 409, "a execução cancelada aceitou retomada")
    _exigir(
        retomada.json().get("motivo") == "execucao_cancelada",
        f"recusa inesperada: {retomada.text}",
    )
    de_novo = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/encerrar")
    _exigir(de_novo.status_code == 409, "encerrar de novo não respondeu 409")
    em_andamento = fluxo.cliente.get("/execucoes/em-andamento")
    _exigir(em_andamento.status_code == 204, "ainda há execução em andamento")


def _cenario_depois_da_tentativa(fluxo: Fluxo) -> None:
    """8x4: encerrar a tentativa mantém a execução; encerrar a execução bloqueia a retomada."""
    fluxo.health_check("8x4")
    fluxo.criar("8x4")
    fluxo.passo(1, 1)
    tentativa = fluxo.cliente.post(
        f"/execucoes/{fluxo.execucao_id}/encerrar-tentativa",
        json={"motivo": "stuck", "observacao": "validação do fluxo"},
    )
    _exigir(
        tentativa.status_code == 200,
        f"encerrar-tentativa: {tentativa.status_code} {tentativa.text}",
    )
    _exigir(
        tentativa.json()["status"] == "em_andamento",
        "encerrar a tentativa cancelou a execução",
    )
    motivo = _tentativa(tentativa.json())["falha"]["motivo"]
    _exigir(motivo == "stuck", "o motivo da tentativa foi perdido")

    execucao = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/encerrar")
    _exigir(
        execucao.status_code == 200,
        f"encerrar execução: {execucao.status_code} {execucao.text}",
    )
    corpo = execucao.json()
    _exigir(corpo["tipo_labirinto"] == "8x4", "o encerramento trocou o labirinto")
    _exigir(corpo["status"] == "cancelada", "a execução não foi cancelada depois da tentativa")
    _exigir(
        _tentativa(corpo)["falha"]["motivo"] == "stuck",
        "o encerramento reescreveu a falha da tentativa",
    )
    retomada = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/retomar")
    _exigir(
        retomada.status_code == 409 and retomada.json().get("motivo") == "execucao_cancelada",
        f"retomada após encerrar a execução: {retomada.status_code} {retomada.text}",
    )


def _cenario_concluida(fluxo: Fluxo) -> None:
    """4x4: o sucesso do firmware conclui a execução e o encerramento manual é recusado."""
    fluxo.health_check("4x4")
    fluxo.criar("4x4")
    fluxo._enviar({"tipo": "sucesso", "x": 3, "y": 3})
    detalhe = fluxo.detalhe()
    _exigir(detalhe["tipo_labirinto"] == "4x4", "o sucesso não permaneceu no 4x4")
    _exigir(detalhe["status"] == "concluida", "o sucesso do firmware não concluiu a execução")
    _exigir(_tentativa(detalhe)["status"] == "success", "a tentativa não ficou success")
    recusa = fluxo.cliente.post(f"/execucoes/{fluxo.execucao_id}/encerrar")
    _exigir(recusa.status_code == 409, "encerrar uma execução concluída não respondeu 409")


def _frontend() -> None:
    alvos = (
        "src/testes/rotas.test.tsx",
        "src/testes/front02.test.tsx",
        "src/testes/front04Integracao.test.tsx",
        "src/testes/labirinto.test.tsx",
    )
    comandos = (
        ["npm", "test", "--", *alvos],
        ["npm", "run", "test:e2e"],
    )
    for comando in comandos:
        print(f"\n$ {' '.join(comando)}", flush=True)
        resultado = subprocess.run(comando, cwd=FRONTEND, check=False)
        if resultado.returncode != 0:
            raise FalhaValidacao(f"frontend falhou: {' '.join(comando)}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8000", help="URL da API")
    parser.add_argument("--sem-frontend", action="store_true", help="só exercita firmware e API")
    args = parser.parse_args(argv)

    cenarios = (
        ("12x4: mapa orientado e execução encerrada", _cenario_labirinto_e_encerrar),
        ("8x4: tentativa encerrada e depois a execução", _cenario_depois_da_tentativa),
        ("4x4: sucesso do firmware recusa encerrar", _cenario_concluida),
    )
    boot = int(time.time()) % 1_000_000
    try:
        with httpx.Client(base_url=args.url, timeout=10) as cliente:
            saude = cliente.get("/back-health")
            _exigir(
                saude.status_code == 200,
                f"API indisponível em {args.url}: {saude.status_code}",
            )
            for nome, cenario in cenarios:
                print(f"• {nome}", flush=True)
                cenario(Fluxo(cliente, boot))
                boot += 1
                print("  ok", flush=True)
        if not args.sem_frontend:
            print("• frontend: desenho do labirinto e botão encerrar execução", flush=True)
            _frontend()
            print("  ok", flush=True)
    except (FalhaValidacao, httpx.HTTPError) as erro:
        print(f"falhou: {erro}", file=sys.stderr)
        return 1
    escopo = "Firmware e API" if args.sem_frontend else "Firmware, API e frontend"
    print(f"\n{escopo} conferidos.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
