"""Validacao do escopo BACK-04 (CalculoMetricas / US13-US16).

Uso, a partir de src/backend:
    python -m scripts.validar_back04
    python -m scripts.validar_back04 -v
    python -m scripts.validar_back04 --apenas-criterios
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

BACKEND_ROOT = Path(__file__).resolve().parents[1]
UNIT = "tests/unit/test_calculo_metricas.py"
INTEG = "tests/integracao/test_metricas_telemetria.py"

MAPA_CRITERIOS = """
Criterios de aceite (BACK-04):
  Consumo mV          test_consumo_bateria_mv
  Consumo % (LiPo)    test_consumo_bateria_pct_pontos_exatos
  Velocidade media    test_velocidade_media + test_velocidade_media_por_celulas
  Divisao por zero    test_velocidade_media_tempo_zero
  Variacao % tempo    test_variacao_melhor_tempo
  Sem historico       test_variacao_sem_historico
  Parcial por tel     test_consumo_parcial_atualiza_a_cada_tel
  Parcial por passo   test_velocidade_e_tempo_parcial_apos_passos
  Finais no sucesso   test_metricas_finais_no_sucesso
"""

ALVOS_COMPLETOS = (UNIT, INTEG)

ALVOS_CRITERIOS = (
    f"{UNIT}::test_consumo_bateria_mv",
    f"{UNIT}::test_consumo_bateria_pct_pontos_exatos",
    f"{UNIT}::test_velocidade_media_tempo_zero",
    f"{UNIT}::test_variacao_melhor_tempo",
    f"{UNIT}::test_variacao_sem_historico",
    f"{UNIT}::test_variacao_melhor_zero_ou_negativo",
    f"{INTEG}::test_consumo_parcial_atualiza_a_cada_tel",
    f"{INTEG}::test_velocidade_e_tempo_parcial_apos_passos",
    f"{INTEG}::test_metricas_finais_no_sucesso",
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
    parser = argparse.ArgumentParser(description="Valida BACK-04 via pytest.")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("--apenas-criterios", action="store_true")
    args = parser.parse_args()

    alvos = ALVOS_CRITERIOS if args.apenas_criterios else ALVOS_COMPLETOS

    print("BACK-04 — validacao automatizada\n", flush=True)
    print(MAPA_CRITERIOS, flush=True)

    codigo = _executar_pytest(alvos, args.verbose)

    print(flush=True)
    if codigo == 0:
        print("Resultado: OK — escopo verificado passou.", flush=True)
    else:
        print(f"Resultado: FALHOU (pytest exit {codigo}).", flush=True)
    return codigo


if __name__ == "__main__":
    sys.exit(main())
