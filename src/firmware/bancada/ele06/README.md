# ELE-06: teste de bancada do microcontrolador, watchdog e I²C

Programa **à parte** da Eletrônica. Não é o firmware do robô e não usa a HAL: só lê `config/pinos.h` e `config/robo.h`, então os pinos são sempre os mesmos do firmware.

## O que ele prova

| Teste | O que verifica | Resultado esperado |
|---|---|---|
| T1 | As saídas de motor (`MOT_STBY`, `PWM`, `IN1`, `IN2`) e o buzzer sobem em 0 | `PASSOU` |
| T2 | SDA (GPIO21) e SCL (GPIO22) em nível alto antes de ligar o I²C | `SDA=1 SCL=1` |
| T3 | Os 3 VL53L0X saem de 0x29 e vão para 0x30 (frontal), 0x31 (esquerdo) e 0x32 (direito) pelo XSHUT | `3/3` |
| T4 | 750 leituras do ID (0xEE) a 400 kHz | `0 erros` |
| Watchdog | Tarefa de controle (núcleo 1) vigiada com timeout de 1 s; ao disparar, zera os pinos de motor dentro da interrupção e reinicia a placa | `Ultimo reset: TASK_WDT` |

## Como gravar

Dentro de `src/firmware`, com o PlatformIO instalado (veja o README do firmware):

```bash
pio run -e ele06_bancada -t upload
pio device monitor
```

No VS Code: na barra inferior, troque o ambiente de `esp32_sim` para `ele06_bancada` e use → (gravar) e 🔌 (monitor).

## Comandos no monitor (115200)

| Tecla | Ação |
|---|---|
| `r` | Refaz os testes |
| `t` | Trava a tarefa de controle de propósito (teste do watchdog) |
| `s` | Status: bateria, ciclos do controle, saídas de motor |
| `b` | Bipe |
| `h` | Ajuda |

## Montagem mínima

- ESP32 alimentada pelo USB, sem motores.
- VL53L0X: VIN→3V3 (nunca 5 V, senão os pull-ups do I²C levam 5 V à ESP32), GND, SDA→GPIO21, SCL→GPIO22, XSHUT→GPIO33 (frontal), GPIO4 (esquerdo), GPIO5 (direito).
- Só com a ESP32, T2 e T3 falham: é esperado (sem módulo, as linhas I²C ficam sem pull-up). T1 e o watchdog já podem ser testados.

## Roteiro de evidências

1. Ligar a placa: `Ultimo reset: POWERON`, LED azul piscando a 1 Hz.
2. Print do resumo (T1 a T4).
3. Tirar o XSHUT de um sensor e digitar `r`: só aquele sensor falha.
4. Digitar `t`: em ~1 s aparece o pânico do watchdog, a placa reinicia e mostra `Ultimo reset: TASK_WDT | resets anormais: 1`.
5. Repetir o passo 4: o contador vai para 2. Ele só zera quando a placa é desligada.
6. Com a ponte H ligada (ELE-13): multímetro no STBY ao digitar `t`; a tensão cai a 0 antes do reinício.
