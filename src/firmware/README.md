# _Firmware_

Firmware do Micromouse Ratatouille, executado na **ESP32 DevKit V1**. A navegação autônoma segue **flood fill (método Adachi)**, conforme a EAP (**4.2.3.2.1**) e o [projeto conceitual de software](../../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md).

Ferramenta: [PlatformIO](https://platformio.org/) com o framework Arduino-ESP32, que dá acesso a `BluetoothSerial`, PCNT, LEDC, NVS e FreeRTOS.

## Instalação

**Opção 1 — VS Code (recomendada):** instale a extensão **PlatformIO IDE** e abra a pasta `src/firmware` (não a raiz do repositório). Os botões ✓ (compilar), → (gravar) e 🔌 (monitor serial) ficam na barra inferior.

**Opção 2 — terminal:** a versão do PlatformIO fica fixada em `requirements.txt` (a mesma do CI).

```bash
cd src/firmware
python3 -m venv .venv
source .venv/bin/activate        # no Windows: .venv\Scripts\activate
pip install -r requirements.txt
pio --version                    # PlatformIO Core, version 6.2.0
```

> [!WARNING]
> Não instale pelo `apt install platformio`: o pacote do Ubuntu traz a versão 4.3.4, que quebra com `AttributeError: 'PlatformioCLI' object has no attribute 'resultcallback'`. Se ela já estiver instalada, remova (`sudo apt remove platformio`) e use o venv acima.

No Linux, para gravar na placa, o usuário precisa estar no grupo `dialout` (`sudo usermod -aG dialout $USER` e depois sair e entrar na sessão).

## Comandos

Rodar dentro de `src/firmware`:

| Comando | O que faz |
|---|---|
| `pio test -e native` | Roda os testes no PC, sem placa |
| `pio run` | Compila para a ESP32 (HAL simulada) |
| `pio run -t upload` | Compila e grava na ESP32 ligada no USB |
| `pio device monitor` | Mostra o que a ESP32 escreve na serial (115200) |
| `make -C simulador` | Roda a navegação em todos os labirintos de `simulador/labirintos/` ([simulador no PC](simulador/README.md)) |

Com a HAL simulada, a ESP32 roda sem nenhum sensor ligado: o LED azul pisca a 2,5 Hz e a serial mostra a telemetria (`tel` a 5 Hz e `heartbeat` a 1 Hz) e linhas `#` de depuração com as distâncias filtradas dos ToF simulados.

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
│   ├── simulador/        corrida célula a célula e roteiro de telemetria (FIRM-02)
│   ├── sensores/         ToF: mediana, paredes com histerese, autoteste e sequência XSHUT (FIRM-03)
│   ├── controle/         PID e movimentos (FIRM-04, FIRM-05)
│   └── navegacao/        flood fill (FIRM-01) — C puro
├── src/
│   ├── main.cpp          setup() e loop() do Arduino
│   └── app/              tarefas FreeRTOS, estados, health-check, telemetria
├── simulador/            programa de PC que roda a navegação nos labirintos (FIRM-02)
│   └── labirintos/       24 labirintos válidos: 4x4, 8x4 e 12x4, espelhados
└── test/                 testes Unity que rodam no PC
```

As camadas só dependem das de baixo: `app → navegacao, controle, sensores → hal`. A `navegacao/` não pode incluir Arduino, FreeRTOS nem a HAL (o CI confere); ela recebe as paredes como parâmetro e devolve o próximo movimento. Assim o mesmo código roda na ESP32, nos testes e no simulador do PC (FIRM-02).

### HAL real e HAL simulada

Cada arquivo da `hal/` é um contrato: diz **o que** o firmware pode pedir ao hardware. A implementação simulada (`hal/sim/`, compilada com `-D HAL_SIMULADA`) responde a esses contratos a partir do mundo simulado: os ToF medem a distância até as paredes do labirinto, os encoders contam o quanto cada roda andou, a bateria descarrega e as rodas travam se o robô bater. Os drivers reais entram ao lado no FIRM-10 (#141) e trocar um pelo outro não muda o resto do código: a lógica acima da HAL (por exemplo, a `lib/sensores/` do FIRM-03) é a mesma nos dois casos.

### Tarefas

| Tarefa | Núcleo | Função |
|---|---|---|
| `navegacao` | 1 | Lê os ToF a 20 Hz; a 5 Hz decide o movimento e põe um evento na fila |
| `telemetria` | 0 | Tira os eventos da fila e envia pelo Bluetooth; nunca bloqueia a navegação |

### Detecção de paredes (FIRM-03)

O robô tem 3 ToF: 1 frontal centralizado, apontado para a frente (0°), e 2 laterais, o esquerdo a +45° e o direito a −45° ([projeto conceitual de estruturas, 4.1](../../docs/4.1%20-%20Projeto%20conceitual%20de%20estruturas.md)). A `lib/sensores/` lê os 3 pela HAL, filtra cada um com a mediana das 3 últimas leituras e decide se há parede à frente, à esquerda e à direita:

- **Frente:** usa só o frontal; com ele falho ou sem leitura, a frente vale "sem parede".
- **Histerese:** vira parede abaixo de `PAREDE_*_ENTRA_MM` e só deixa de ser acima de `PAREDE_*_SAI_MM` (`config/robo.h`). Entre os dois, mantém o estado anterior, para o ruído não fazer a parede "piscar".
- **Nada à vista** (o VL53L0X devolve ~8190) é saturado em `TOF_ALCANCE_MAX_MM`: corredor aberto, não falha.
- **Falha:** 3 leituras seguidas sem resposta marcam o sensor como falho até `sensores_iniciar()`, e ele conta como "sem parede". Quem gera a `falha_componente` é a FIRM-09.
- **Autoteste** (`tof_autoteste_executar`): 5 leituras por sensor; dá nome, `aprovado` e a mediana em mm (ou nulo, se nada respondeu) para o `hc_item`.
- **XSHUT** (`tof_sequencia_inicio`): descreve a ordem para ligar um sensor por vez e trocar os endereços dos 3 sensores para 0x30 a 0x32. O driver real (FIRM-10) só percorre a lista.

> [!IMPORTANT]
> O ToF lateral aponta 45° para a frente, então mede a parede **~74 mm à frente** do centro do robô, e não ao lado dele. Com parede, lê ~77 mm. Sem parede, o feixe atravessa o lado aberto e bate no que houver na célula vizinha. Medido no mundo simulado, no pior caso (vizinha com parede horizontal), com *p* = quanto o robô já entrou na célula:
>
> | *p* (mm) | Sem parede lê | Resultado |
> |---|---|---|
> | −65 a +30 | 310 a 176 mm | confiável |
> | +35 a +85 | 169 a 98 mm | parede falsa (parede da vizinha) |
> | +90 a +110 | 91 a 77 mm | parede falsa (poste do canto) |
> | acima de +115 | — | já mede a célula seguinte |
>
> A parede lateral de uma célula deve ser guardada **ao entrar nela** (*p* entre 0 e ~20 mm); as 3 amostras da mediana vêm do trecho logo antes da fronteira, que também é confiável. Quem escolhe esse momento é o movimento (FIRM-05) ou a navegação (FIRM-01).

### Labirintos em texto

A simulação lê labirintos desenhados assim. No [simulador do PC](simulador/README.md), a largada é a célula marcada com `L` (sem a marca, a do canto inferior esquerdo) e o robô começa virado para a saída dela; o mundo simulado da ESP32 sempre começa em (0, 0), virado para o norte:

```text
+---+---+---+---+
|           |   |
+   +---+   +   +
|   |           |
+   +   +---+   +
|   |   |       |
+   +---+   +---+
| L |           |
+---+---+---+---+
```

## Pendências

- **Pinos:** `config/pinos.h` segue a folha de Controle do esquemático (ARQ-09). A ESP32 de 30 pinos tem 23 GPIOs livres e o robô pede 24 sinais: o esquemático fecha a conta porque ainda usa as 5 redes do A4988, mas a TB6612 pede 7, e `MOT_D_IN2` ficou sem pino. Uma saída é ligar PWMA/PWMB em nível alto e fazer o PWM nas linhas IN. A decisão é da Eletrônica, que também precisa trocar o pull-up de `MOT_EN` (R1) por pull-down, para a TB6612 não ligar os motores no boot.
- Ainda provisórios em `config/robo.h`: pulsos por volta do encoder e posição x/y dos ToF (Estrutura), as curvas de descarga da bateria (Energia, até os testes da 7.2) e os limiares de parede dos ToF (`PAREDE_*_MM`, tirados da geometria; calibrar na pista no FIRM-10). O formato das mensagens é provisório até o ARQ-01.
- **Bancada (ARQ-09):** montar ESP32 + ToF + motor com encoder + ponte H quando os componentes chegarem; é nela que se confirmam os valores provisórios acima.

> [!WARNING]
> **Não acrescente arquivos referentes a _hardware_ nesta pasta.** Eles deverão ser armazenados na pasta [hw](../../hw) deste repositório. Também não versione a saída da compilação (`.pio/`, `.bin`, `.elf`).
