# Resultados da simulação

Gerado por `simular.py`. Premissas e conclusões no [README](README.md).

## Transiente: cruzeiro e rodas travadas a partir de 10 ms

| Cenário | Fonte | Ciclo máx. | VBAT mín. em cruzeiro | VBAT mín. travado | Corrente média no PTC travado | Pico no PTC | Queda média no SS34 | Ondulação na VBAT | Ondulação no ADC | Tensão média no motor travado |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| LiPo cheia | 8,4 V | 71% | 8,02 V | 7,78 V | 1,14 A | 1,50 A | 0,38 V | 172 mV | 8,9 mV | 5,58 V |
| LiPo no alerta | 7,4 V | 81% | 7,01 V | 6,75 V | 1,30 A | 1,60 A | 0,39 V | 160 mV | 9,9 mV | 5,52 V |
| LiPo no corte | 7,0 V | 86% | 6,61 V | 6,34 V | 1,38 A | 1,65 A | 0,39 V | 151 mV | 10,3 mV | 5,51 V |
| Pilhas AA novas | 6,0 V | 100% | 5,31 V | 4,44 V | 1,44 A | 1,64 A | 0,40 V | 240 mV | 48,6 mV | 4,60 V |
| Pilhas AA no alerta | 5,0 V | 100% | 3,88 V | 2,51 V | 1,22 A | 1,60 A | 0,38 V | 737 mV | 159,4 mV | 3,03 V |

VBAT abaixo de 3,5 V indica que o buck-boost desligaria. A partir daí a carga simulada deixa de representar o circuito, e o valor serve só para mostrar a falha.

## Limite das pilhas com as rodas travadas

- Resistência interna de 0,6 Ω: o buck-boost recebe menos de 3,5 V com as pilhas abaixo de **4,96 V**.
- Resistência interna de 1,2 Ω: o buck-boost recebe menos de 3,5 V com as pilhas abaixo de **5,87 V**.

## Tensão no ADC com 0,43 A de consumo médio

| Ponto | Fonte | Depois do SS34 | No ADC |
|---|:--:|:--:|:--:|
| LiPo cheia | 8,4 V | 8,01 V | 2,50 V |
| Alerta da LiPo | 7,4 V | 7,01 V | 2,19 V |
| Corte da LiPo | 7,0 V | 6,61 V | 2,07 V |
| Pilhas AA novas | 6,0 V | 5,38 V | 1,68 V |
| Alerta das pilhas | 5,0 V | 4,12 V | 1,29 V |

## Filtro do divisor

- Frequência de corte: 231 Hz
- Atenuação adicional a 20 kHz (PWM): -38,7 dB

## VIN do ESP32 depois do D1 (1N5819)

- ESP32 transmitindo (240 mA): 4,74 V
- ESP32 transmitindo e ToF, encoders e TB6612 no 3V3 do DevKit (380 mA): 4,71 V
