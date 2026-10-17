"""Validação do escopo BACK-03 (GerenciadorExecucoes / regras TAP).

Executa pytest de integração e relaciona o resultado aos critérios de aceite
e à tabela docs/decisoes/transicoes-execucao.md.

Requisitos: TEST_DATABASE_URL apontando para banco *_test (igual ao CI).

Uso, a partir de src/backend:
    python -m scripts.validar_back03
    python -m scripts.validar_back03 -v
    python -m scripts.validar_back03 --apenas-criterios
    python -m scripts.validar_back03 --mostrar-mapa
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

BACKEND_ROOT = Path(__file__).resolve().parents[1]
TRANSICOES = "tests/integracao/test_transicoes_execucao.py"
ROTAS = "tests/integracao/test_rotas_execucoes.py"

MAPA_ORACULO = """
Oráculo transicoes-execucao.md → testes
  T01  test_t01_nova_execucao_abre_health_check
  T02  test_t02_retomar_abre_proxima_tentativa
  T03  test_t03_hc_aprovado_vai_para_running
  T04  test_t04_hc_reprovado_falha
  T05  test_t05_sucesso_conclui_execucao
  T06  test_t06_falha_trajeto_mantem_em_andamento
  T07  test_t07_encerrar_manual
  T08  test_t08_link_lost_cancela_execucao
  T09  test_t09_tempo_excedido_cancela_execucao
  T10  test_t10_failed_e_terminal
  T11  test_t11_success_e_terminal
  T12  test_t12_terceira_falha_cancela_execucao
  T13  test_t13_nova_execucao_cancela_anterior
  E01  test_e01_nova_sem_anterior
  E02  test_e02_nova_cancela_anterior
  E03  test_e03_sucesso_conclui
  E04  test_e04_falha_com_retomada
  E05  test_e05_terceira_falha_cancela
  E06  test_e06_perda_link_cancela
  E07  test_e07_tempo_execucao_cancela
  E08  test_e08_nova_apos_terminal
  R01  test_r01_tentativa_aberta
  R02  test_r02_retomar_execucao_nao_em_andamento
  R03  test_r03_limite_3
  R04  test_r04_encerrar_execucao_cancelada
  R05  test_r05_encerrar_sem_tentativa_aberta
"""

MAPA_CRITERIOS = """
Critérios de aceite (task BACK-03):
  CA-1  Um teste por linha T/E/R da tabela (ver --mostrar-mapa)
  CA-2  test_retomar_concorrente_uma_aceita_outra_recusa (tentativa_aberta)
  CA-3  test_r03_limite_3
  CA-4  test_t05_sucesso_conclui_execucao + test_sucesso_via_telemetria_http

Escopo complementar:
  Rotas HTTP → test_rotas_execucoes.py
  Confirmação pós-encerrar (sem 2ª FALHA) → test_confirmacao_web_nao_duplica_falha
"""

ALVOS_COMPLETOS = (TRANSICOES, ROTAS)

ALVOS_CRITERIOS = (
    f"{TRANSICOES}::TestTransicoesTentativa",
    f"{TRANSICOES}::TestTransicoesExecucao",
    f"{TRANSICOES}::TestRecusas",
    f"{TRANSICOES}::test_retomar_concorrente_uma_aceita_outra_recusa",
    f"{TRANSICOES}::test_sucesso_via_telemetria_http",
)


def _executar_pytest(alvos: tuple[str, ...], verbose: bool) -> int:
    base = [sys.executable, "-m", "pytest", *alvos]
    if verbose:
        cmd = base + ["--tb=short", "-v"]
    else:
        cmd = base + ["--tb=line", "-q"]
    print("Comando:", " ".join(cmd), "\n", flush=True)
    return subprocess.run(cmd, cwd=BACKEND_ROOT, check=False).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description="Valida BACK-03 via pytest de integração.")
    parser.add_argument("-v", "--verbose", action="store_true", help="Pytest verboso.")
    parser.add_argument(
        "--apenas-criterios",
        action="store_true",
        help="Só testes mapeados aos critérios de aceite (sem test_rotas_execucoes).",
    )
    parser.add_argument(
        "--mostrar-mapa",
        action="store_true",
        help="Imprime o mapa T/E/R e sai (não roda pytest).",
    )
    args = parser.parse_args()

    if args.mostrar_mapa:
        print("BACK-03 — mapa oráculo → pytest\n", flush=True)
        print(MAPA_ORACULO, flush=True)
        print(MAPA_CRITERIOS, flush=True)
        return 0

    alvos = ALVOS_CRITERIOS if args.apenas_criterios else ALVOS_COMPLETOS

    print("BACK-03 — validação automatizada\n", flush=True)
    print(MAPA_CRITERIOS, flush=True)
    print("(Mapa completo T01–R05: python -m scripts.validar_back03 --mostrar-mapa)\n", flush=True)

    codigo = _executar_pytest(alvos, args.verbose)

    print(flush=True)
    if codigo == 0:
        print("Resultado: OK — escopo verificado passou.", flush=True)
    else:
        print(f"Resultado: FALHOU (pytest exit {codigo}).", flush=True)
        print("Investigue com:", flush=True)
        print(f"  cd {BACKEND_ROOT} && python -m pytest {TRANSICOES} {ROTAS} -v", flush=True)
    return codigo


if __name__ == "__main__":
    sys.exit(main())
