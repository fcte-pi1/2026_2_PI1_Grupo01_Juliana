"""Simulação do caminho de alimentação do Micromouse em ngspice (cronograma 2.2.1, #222).

Monta o netlist de cada cenário, roda o ngspice em modo batch e imprime os
resultados em Markdown. Uso: python simular.py > resultados.md
"""

from __future__ import annotations

import math
import re
import subprocess
import tempfile
from pathlib import Path

# Premissas do circuito (4.2 e folha "Alimentação e Motores" da Eletrônica)
R_CONTATOS = 0.02  # chave geral e XT30
R_PTC = 0.08  # JK30-200 frio, pior caso adotado (confirmar na folha de dados)
R_CANAL = 7.5 + 0.5  # N20 travado (6 V / 0,80 A) mais Ron da TB6612
I_CRUZEIRO = 0.30  # dois motores a 150 mA
ETA_BB = 0.85  # rendimento do buck-boost
I_5V_BASE = 0.20  # ESP32 120 mA, LDO com ToF e encoders 76 mA, quiescente 5 mA
I_5V_PICO = 0.18  # rádio a 240 mA e ToF a 40 mA
F_PWM = 20e3
UVLO_BB = 3.5  # menor entrada aceita pelo buck-boost (4.2, Tabela 7)

MODELOS = """
.model SS34 D(IS=1e-5 N=1.1 RS=0.04 BV=40 CJO=200p)
.model D1N5819 D(IS=3e-5 N=1.05 RS=0.08 BV=40 CJO=110p)
"""

CASOS = [
    # nome, tensão da fonte, resistência interna, ciclo máximo (4.2, Tabela 8)
    ("LiPo cheia", 8.4, 0.05, 0.71),
    ("LiPo no alerta", 7.4, 0.05, 0.81),
    ("LiPo no corte", 7.0, 0.05, 0.86),
    ("Pilhas AA novas", 6.0, 0.6, 1.0),
    ("Pilhas AA no alerta", 5.0, 1.2, 1.0),
]


def netlist(vsrc: float, rint: float, duty: float, analise: str, transiente: bool) -> str:
    if transiente:
        periodo = 1 / F_PWM
        pwm = f"VPWM pwm 0 PULSE(0 1 0 1u 1u {duty * periodo - 2e-6:.3e} {periodo:.3e})"
        radio = "VRAD rad 0 PULSE(0 1 2m 10u 10u 2m 10m)"
        parada = "VST st 0 PULSE(0 1 10m 100u 100u 1 2)"
    else:
        pwm = f"VPWM pwm 0 DC {duty}"
        radio = "VRAD rad 0 DC 1"
        parada = "VST st 0 DC 1"
    return f"""* Caminho de alimentação
VSRC src 0 DC {vsrc}
RINT src n1 {rint}
RCON n1 n2 {R_CONTATOS}
VAMP n2 n2a 0
RPTC n2a n3 {R_PTC}
DSS34 n3 vbat SS34
C2 vbat c2 100u
RC2 c2 0 0.1
* Ponte H: a corrente do motor é suavizada pela indutância (filtro de 1 ms)
RF vbat vbf 1k
CF vbf 0 1u
{pwm}
{parada}
BMOT vbat 0 I = V(pwm) * ((1 - V(st)) * {I_CRUZEIRO} + V(st) * 2 * {duty} * V(vbf) / {R_CANAL})
* Buck-boost como carga de potência constante, com capacitor de entrada
CBB vbat 0 22u
{radio}
BBB vbat 0 I = 5 * ({I_5V_BASE} + {I_5V_PICO} * V(rad)) / ({ETA_BB} * max(V(vbat), 2))
* Divisor do ADC com filtro
R1 vbat sense 22k
R2 sense 0 10k
C3 sense 0 100n
{MODELOS}
{analise}
.end
"""


def rodar(texto: str) -> dict[str, float]:
    with tempfile.TemporaryDirectory() as pasta:
        arquivo = Path(pasta) / "circuito.cir"
        arquivo.write_text(texto, encoding="utf-8")
        saida = subprocess.run(
            ["ngspice", "-b", str(arquivo)], capture_output=True, text=True, check=True
        ).stdout
    return {
        nome.lower(): float(valor)
        for nome, valor in re.findall(r"^([\w()]+)\s*=\s*([-+0-9.eE]+)", saida, re.MULTILINE)
    }


def transiente(vsrc: float, rint: float, duty: float) -> dict[str, float]:
    medidas = """
.tran 0.5u 30m 0 0.5u
.meas tran vbat_cruz_min MIN V(vbat) FROM=5m TO=10m
.meas tran vbat_trav_min MIN V(vbat) FROM=12m TO=30m
.meas tran vbat_trav_avg AVG V(vbat) FROM=12m TO=30m
.meas tran vbat_trav_pp PP V(vbat) FROM=12m TO=30m
.meas tran iptc_trav_avg AVG I(VAMP) FROM=12m TO=30m
.meas tran iptc_trav_max MAX I(VAMP) FROM=12m TO=30m
.meas tran vn3_trav_avg AVG V(n3) FROM=12m TO=30m
.meas tran vsense_trav_pp PP V(sense) FROM=20m TO=30m
"""
    return rodar(netlist(vsrc, rint, duty, medidas, transiente=True))


def limiar_pilhas(rint: float) -> float:
    """Menor tensão das pilhas que mantém o buck-boost acima do UVLO com as rodas travadas."""
    medidas = f"""
.dc VSRC 3.5 7 0.01
.meas dc limiar FIND V(src) WHEN V(vbat)={UVLO_BB} RISE=1
"""
    return rodar(netlist(5.0, rint, 1.0, medidas, transiente=False))["limiar"]


def leitura_adc(vsrc: float, rint: float, corrente: float) -> tuple[float, float]:
    """Tensão depois do SS34 e no ADC com a corrente média de operação."""
    texto = f"""* Divisor em regime
VSRC src 0 DC {vsrc}
RINT src n1 {rint + R_CONTATOS + R_PTC}
DSS34 n1 vbat SS34
ICARGA vbat 0 DC {corrente}
R1 vbat sense 22k
R2 sense 0 10k
{MODELOS}
.control
op
print v(vbat) v(sense)
.endc
.end
"""
    medidas = rodar(texto)
    return medidas["v(vbat)"], medidas["v(sense)"]


def filtro_adc() -> tuple[float, float]:
    texto = f"""* Filtro do divisor
VIN vbat 0 DC 0 AC 1
R1 vbat sense 22k
R2 sense 0 10k
C3 sense 0 100n
.control
ac dec 200 1 1meg
meas ac ganho_dc find vdb(sense) at=1
meas ac fc when vdb(sense)={20 * math.log10(10 / 32) - 3:.3f} fall=1
meas ac ganho_pwm find vdb(sense) at={F_PWM}
.endc
.end
"""
    medidas = rodar(texto)
    return medidas["fc"], medidas["ganho_pwm"] - medidas["ganho_dc"]


def vin_esp32(corrente: float) -> float:
    """VIN do DevKit depois do D1 (1N5819), com a linha de 5 V ideal."""
    texto = f"""* Linha de 5 V até o VIN
V5 n5 0 DC 5
D1 n5 vin D1N5819
ICARGA vin 0 DC {corrente}
{MODELOS}
.control
op
print v(vin)
.endc
.end
"""
    return rodar(texto)["v(vin)"]


def br(valor: float, casas: int = 2) -> str:
    return f"{valor:.{casas}f}".replace(".", ",")


def main() -> None:
    print("# Resultados da simulação\n")
    print("Gerado por `simular.py`. Premissas e conclusões no [README](README.md).\n")
    print("## Transiente: cruzeiro e rodas travadas a partir de 10 ms\n")
    print(
        "| Cenário | Fonte | Ciclo máx. | VBAT mín. em cruzeiro | VBAT mín. travado "
        "| Corrente média no PTC travado | Pico no PTC | Queda média no SS34 "
        "| Ondulação na VBAT | Ondulação no ADC | Tensão média no motor travado |"
    )
    print("|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|")
    for nome, vsrc, rint, duty in CASOS:
        m = transiente(vsrc, rint, duty)
        print(
            f"| {nome} | {br(vsrc, 1)} V | {duty:.0%} | {br(m['vbat_cruz_min'])} V "
            f"| {br(m['vbat_trav_min'])} V | {br(m['iptc_trav_avg'])} A "
            f"| {br(m['iptc_trav_max'])} A | {br(m['vn3_trav_avg'] - m['vbat_trav_avg'])} V "
            f"| {br(m['vbat_trav_pp'] * 1000, 0)} mV | {br(m['vsense_trav_pp'] * 1000, 1)} mV "
            f"| {br(duty * m['vbat_trav_avg'])} V |"
        )
    print(
        f"\nVBAT abaixo de {br(UVLO_BB, 1)} V indica que o buck-boost desligaria. A partir daí "
        "a carga simulada deixa de representar o circuito, e o valor serve só para mostrar a falha."
    )

    print("\n## Limite das pilhas com as rodas travadas\n")
    for rint in (0.6, 1.2):
        print(
            f"- Resistência interna de {br(rint, 1)} Ω: o buck-boost recebe menos de "
            f"{br(UVLO_BB, 1)} V com as pilhas abaixo de **{br(limiar_pilhas(rint))} V**."
        )

    print("\n## Tensão no ADC com 0,43 A de consumo médio\n")
    print("| Ponto | Fonte | Depois do SS34 | No ADC |")
    print("|---|:--:|:--:|:--:|")
    for nome, vsrc, rint in [
        ("LiPo cheia", 8.4, 0.05),
        ("Alerta da LiPo", 7.4, 0.05),
        ("Corte da LiPo", 7.0, 0.05),
        ("Pilhas AA novas", 6.0, 0.6),
        ("Alerta das pilhas", 5.0, 1.2),
    ]:
        vbat, vsense = leitura_adc(vsrc, rint, 0.43)
        print(f"| {nome} | {br(vsrc, 1)} V | {br(vbat)} V | {br(vsense)} V |")

    fc, atenuacao = filtro_adc()
    print("\n## Filtro do divisor\n")
    print(f"- Frequência de corte: {br(fc, 0)} Hz")
    print(f"- Atenuação adicional a 20 kHz (PWM): {br(atenuacao, 1)} dB")

    print("\n## VIN do ESP32 depois do D1 (1N5819)\n")
    for descricao, corrente in [
        ("ESP32 transmitindo (240 mA)", 0.24),
        ("ESP32 transmitindo e ToF, encoders e TB6612 no 3V3 do DevKit (380 mA)", 0.38),
    ]:
        print(f"- {descricao}: {br(vin_esp32(corrente))} V")


if __name__ == "__main__":
    main()
