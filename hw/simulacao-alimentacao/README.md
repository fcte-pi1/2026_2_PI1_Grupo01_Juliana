# Simulação do circuito de alimentação

Simulação em ngspice do caminho de alimentação, da fonte até a VBAT, o divisor do ADC e o VIN do ESP32 (cronograma 2.2.1, issue #222). O circuito segue a folha "Alimentação e Motores" da Eletrônica, com o PTC que o projeto conceitual de energia (4.2) prevê e que ainda falta no esquemático.

## Como rodar

Precisa do Python 3.10+ e do ngspice no PATH (`apt install ngspice`, ou o instalador do site no Windows).

```bash
python simular.py > resultados.md
```

O script gera o netlist de cada cenário, roda o ngspice em modo batch e escreve as tabelas de [`resultados.md`](resultados.md).

## Circuito simulado

```
fonte (Rint) ─ chave + XT30 ─ PTC ─ SS34 ─┬─ VBAT ─ C2 100 µF
                                          ├─ ponte H + 2 × N20 (PWM a 20 kHz)
                                          ├─ buck-boost 5 V (22 µF na entrada)
                                          └─ 22 kΩ ─┬─ ADC
                                                    └─ 10 kΩ ∥ 100 nF
buck-boost 5 V ─ D1 (1N5819) ─ VIN do ESP32
```

## Premissas

| Item | Valor adotado | Origem |
|---|---|---|
| Resistência interna da LiPo | 0,05 Ω | Pior caso para a Tattu 650 mAh 75C, com fiação |
| Resistência interna das 4 pilhas AA | 0,6 Ω (novas) e 1,2 Ω (gastas) | 4.2, seção 4.3 |
| Chave geral e XT30 | 0,02 Ω | Estimativa |
| PTC JK30-200 | 0,08 Ω | Pior caso adotado; confirmar na folha de dados |
| SS34 | Modelo ajustado a 0,5 V em 3 A | Folha de dados do SS34 |
| 1N5819 | Modelo ajustado a 0,55 V em 3 A | Folha de dados do 1N5819 |
| Motor N20 travado | 7,5 Ω por motor (6 V / 0,80 A) mais 0,5 Ω da TB6612 | 4.2, seção 2.1 |
| Motores em cruzeiro | 0,30 A no conjunto | 4.2, Tabela 1 |
| Ciclo de trabalho máximo | 71%, 81%, 86% e 100% | 4.2, Tabela 8 |
| Carga da linha de 5 V | 0,20 A, com picos de 0,38 A por 2 ms a cada 10 ms (rádio e ToF) | 4.2, Tabelas 1 e 7 |
| Buck-boost | Carga de potência constante com 85% de rendimento; desliga abaixo de 3,5 V na entrada | 4.2, Tabela 7 |

O buck-boost e o LDO não foram modelados por dentro, porque o modelo do buck-boost ainda não foi escolhido. A ondulação da linha de 5 V e o critério de ±5% ficam para o teste 2.11 da página 7.2.

## Conclusões

1. **Com a LiPo, o circuito atende em todos os pontos.** Com as rodas travadas e a bateria no corte, a VBAT mínima é de 6,34 V, bem acima dos 3,5 V do buck-boost. A corrente no PTC fica em até 1,38 A de média e 1,65 A de pico, abaixo do hold de 2,0 A. Um travamento, portanto, não desarma o fusível.
2. **A queda entre a fonte e o ADC é de cerca de 0,39 V com a LiPo e 0,43 A.** São 0,33 V no SS34 e o resto na fiação e no PTC. A Tabela 10 do 4.2 bate com a simulação em até 0,02 V. O valor de 0,35 V do firmware é adequado como ponto de partida e será calibrado no teste 2.4.
3. **Com as pilhas, a Tabela 10 do 4.2 não considera a resistência interna.** Com 0,43 A, as pilhas novas aparecem no ADC como 1,68 V, e não como 1,77 V. No alerta de 5,0 V a leitura é de 1,29 V, e não de 1,45 V. O limite de alerta das pilhas no firmware precisa considerar essa queda, ou ser comparado com a leitura com os motores parados.
4. **Com as pilhas e as rodas travadas, a lógica pode reiniciar.** A entrada do buck-boost cai abaixo de 3,5 V quando as pilhas estão abaixo de 4,96 V (novas) ou de 5,87 V (gastas). Os 100 µF de C2 seguram só alguns milissegundos, muito menos que o 1 s da detecção de bloqueio. Para manter o RNF01 com as pilhas, o firmware precisa reduzir o ciclo máximo quando a fonte ativa for a de pilhas, ou detectar o bloqueio mais rápido. Se nada disso for feito, as pilhas ficam só como contingência para o robô parado ou em baixa velocidade.
5. **O filtro do divisor corta em 231 Hz e atenua o PWM de 20 kHz em mais 38,7 dB.** Com a LiPo, a ondulação no ADC fica em torno de 10 mV, cerca de 0,4% da leitura, e a média móvel de 8 amostras dá conta dela.
6. **O VIN do ESP32 fica entre 4,71 e 4,74 V depois do D1.** O regulador de 3,3 V do DevKit precisa de cerca de 4,4 V, então a folga é de uns 0,3 V. Se o buck-boost entregar 5 V − 5%, o VIN cai para cerca de 4,45 V e a folga praticamente acaba. Isso reforça o LDO dedicado de 3,3 V do 4.2 para os sensores, no lugar do pino 3V3 do DevKit.
