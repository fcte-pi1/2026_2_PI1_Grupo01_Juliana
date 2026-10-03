# _Firmware_

Firmware do Micromouse Ratatouille, executado na **ESP32 DevKit V1**. A navegação autônoma segue **flood fill (método Adachi)**, conforme a EAP (**4.2.3.2.1**) e o [projeto conceitual de software](../../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md).

Ferramenta: [PlatformIO](https://platformio.org/) com o framework Arduino-ESP32, que dá acesso a `BluetoothSerial`, PCNT, LEDC, NVS e FreeRTOS.

## Instalação

**Opção 1 — VS Code (recomendada):** instale a extensão **PlatformIO IDE** e abra a pasta `src/firmware` (não a raiz do repositório). Os botões ✓ (compilar), → (gravar) e 🔌 (monitor serial) ficam na barra inferior.

**Opção 2 — terminal:**

```bash
python3 -m venv ~/.platformio/penv
~/.platformio/penv/bin/pip install platformio
ln -s ~/.platformio/penv/bin/pio ~/.local/bin/pio
```

No Linux, para gravar na placa, o usuário precisa estar no grupo `dialout` (`sudo usermod -aG dialout $USER` e depois sair e entrar na sessão).

## Comandos

Rodar dentro de `src/firmware`:

| Comando | O que faz |
|---|---|
| `pio test -e native` | Roda os testes no PC, sem placa |
| `pio run` | Compila para a ESP32 (HAL simulada) |
| `pio run -t upload` | Compila e grava na ESP32 ligada no USB |
| `pio device monitor` | Mostra o que a ESP32 escreve na serial (115200) |

Com a HAL simulada, a ESP32 roda sem nenhum sensor ligado: o LED azul pisca a 2,5 Hz e a serial mostra a telemetria (`tel` a 5 Hz e `heartbeat` a 1 Hz) e linhas `#` de depuração com as leituras dos ToF simulados.

### Sem placa: simulador Wokwi

O [Wokwi](https://wokwi.com) simula a ESP32 e roda o mesmo `firmware.bin` que iria para a placa. A configuração já está em `wokwi.toml` e `diagram.json`.

1. No VS Code, instale a extensão **Wokwi Simulator**.
2. `F1` → **Wokwi: Request a New License** (abre o navegador; a licença é gratuita, basta criar a conta). Só na primeira vez.
3. Compile com `pio run`.
4. Com a pasta `src/firmware` aberta, `F1` → **Wokwi: Start Simulator**.

A saída serial aparece no terminal do VS Code. O Wokwi não simula Bluetooth, mas com a HAL simulada a telemetria sai pela serial.

## Organização

```text
src/firmware/
├── platformio.ini        ambientes: esp32_sim (ESP32) e native (PC/testes)
├── include/config/       pinos.h e robo.h (valores e fontes do ARQ-09)
├── lib/
│   ├── hal/              contratos do hardware (hal_tof.h, hal_motor.h, ...)
│   │   └── sim/          implementação simulada de cada contrato
│   ├── simulacao/        mundo simulado: labirinto em texto + física do robô
│   ├── controle/         PID e movimentos (FIRM-04, FIRM-05)
│   └── navegacao/        flood fill (FIRM-01) — C puro
├── src/
│   ├── main.cpp          setup() e loop() do Arduino
│   └── app/              tarefas FreeRTOS, estados, health-check, telemetria
└── test/                 testes Unity que rodam no PC
```

As camadas só dependem das de baixo: `app → navegacao, controle → hal`. A `navegacao/` não pode incluir Arduino, FreeRTOS nem a HAL (o CI confere); ela recebe as paredes como parâmetro e devolve o próximo movimento. Assim o mesmo código roda na ESP32, nos testes e no simulador do PC (FIRM-02).

### HAL real e HAL simulada

Cada arquivo da `hal/` é um contrato: diz **o que** o firmware pode pedir ao hardware. A implementação simulada (`hal/sim/`, compilada com `-D HAL_SIMULADA`) responde a esses contratos a partir do mundo simulado: os ToF medem a distância até as paredes do labirinto, os encoders contam o quanto cada roda andou, a bateria descarrega e as rodas travam se o robô bater. Os drivers reais entram ao lado (FIRM-03 a FIRM-07) e trocar um pelo outro não muda o resto do código.

### Tarefas

| Tarefa | Núcleo | Função |
|---|---|---|
| `navegacao` | 1 | Lê os sensores, decide o movimento e põe um evento na fila (5 Hz) |
| `telemetria` | 0 | Tira os eventos da fila e envia pelo Bluetooth; nunca bloqueia a navegação |

### Labirintos em texto

A simulação lê labirintos desenhados assim; a largada é a célula do canto inferior esquerdo, virada para o norte:

```text
+---+---+---+---+
|           |   |
+   +---+   +   +
|   |       |   |
+   +   +---+   +
|   |   |       |
+   +---+   +---+
|   |           |
+---+---+---+---+
```

## Pendências

- **Pinos:** `config/pinos.h` segue a folha de Controle do esquemático (ARQ-09). A ESP32 de 30 pinos tem 23 GPIOs livres e o robô pede 24 sinais: o esquemático fecha a conta porque ainda usa as 5 redes do A4988, mas a TB6612 pede 7, e `MOT_D_IN2` ficou sem pino. Uma saída é ligar PWMA/PWMB em nível alto e fazer o PWM nas linhas IN. A decisão é da Eletrônica, que também precisa trocar o pull-up de `MOT_EN` (R1) por pull-down, para a TB6612 não ligar os motores no boot.
- Ainda provisórios em `config/robo.h`: pulsos por volta do encoder e posição x/y dos ToF. O formato das mensagens é provisório até o ARQ-01.

> [!WARNING]
> **Não acrescente arquivos referentes a _hardware_ nesta pasta.** Eles deverão ser armazenados na pasta [hw](../../hw) deste repositório. Também não versione a saída da compilação (`.pio/`, `.bin`, `.elf`).
