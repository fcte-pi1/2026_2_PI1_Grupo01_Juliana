# _Firmware_

Firmware do Micromouse Ratatouille, executado na **ESP32 DevKit V1**. A navegação autônoma segue **flood fill (método Adachi)**, conforme a EAP (**4.2.3.2.1**) e o [projeto conceitual de software](../../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md).

Ferramenta: [PlatformIO](https://platformio.org/) com o framework Arduino-ESP32, que dá acesso a `BluetoothSerial`, PCNT, LEDC, NVS e FreeRTOS.

## Instalação

**Opção 1, VS Code (recomendada):** instale a extensão **PlatformIO IDE** e abra a pasta `src/firmware` (não a raiz do repositório). Os botões de compilar, gravar e abrir o monitor serial ficam na barra inferior.

**Opção 2, terminal:** a versão do PlatformIO fica fixada em `requirements.txt` (a mesma do CI).

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
| `pio test -e native_cov` | Os mesmos testes, gerando os dados de cobertura ([cobertura](#cobertura-dos-testes)) |
| `pio run` | Compila para a ESP32 (HAL simulada) |
| `pio run -t upload` | Compila e grava na ESP32 ligada no USB |
| `pio run -e esp32_drivers -t upload` | Programa de bancada com os drivers reais: gira cada motor por comando na serial e mostra encoders e bateria ([bancada/drivers](bancada/drivers/main.cpp)) |
| `pio run -e esp32_real_hc -t upload` | Firmware com a HAL real, **só o health-check**, com os ToF reais. Reprova enquanto os ToF laterais não funcionarem |
| `pio run -e esp32_real_hc_tof_mock -t upload` | O mesmo, com o ToF mockado (leituras fixas): o health-check aprova e serve para o e2e no cenário ideal |
| `pio run -e esp32_real -t upload` | **Firmware completo no robô**: HAL real, navegação, movimento pelos motores, health-check, BOOT e telemetria pela serial USB. Anda e para: decide parado no centro da célula, gira e avança. Só com o interruptor da bateria ligado, robô na pista |
| `pio run -e esp32_real_tof_mock -t upload` | O mesmo, com o ToF mockado (frontal 300 mm, laterais 120 mm), para andar com os ToF laterais em falta |
| `pio run -e esp32_real_bt -t upload` | O firmware completo com o **Bluetooth real**: a telemetria e os comandos vão pelo SPP "Ratatouille", sem cabo USB ([Bluetooth real](#bluetooth-real-firm-11)) |
| `pio run -e esp32_real_bt_tof_mock -t upload` | O mesmo, com o ToF mockado |
| `pio run -e esp32_calibracao_bt -t upload` | A bancada de calibração com as mesmas teclas pelo USB e pelo Bluetooth, para calibrar na pista sem cabo |
| `pio run -e esp32_calibracao -t upload` | Bancada de calibração no robô montado: avanço de 1 e de 10 células, giros de 90°, 180° e 10 giros, velocidade, stuck e leitura dos ToF com as paredes decididas ([calibração](#calibração-no-robô)) |
| `pio run -e esp32_sim_boot -t upload` | Igual ao `esp32_sim`, mas com o botão BOOT físico da placa e a navegação a 1 Hz (uma célula por segundo): dá tempo de apertar o BOOT no meio da corrida simulada |
| `python tools/monitor_comandos.py` | Monitor serial que manda os comandos do contrato por tecla (`l` largar, `i` interromper, `h` health_check, `t` mostra toda a `tel`, `q` sai) e só imprime a `tel` quando o estado muda |
| `pio run -e esp32_hc_bancada -t upload` | Bancada interativa por comandos na serial (`d` DIP, `b` bateria, `t` ToF, `r` reinicia os ToF sem reiniciar a placa, `m` motores, `h` health-check, `i` varredura do I²C, `x`, `f` e `v` para diagnosticar o XSHUT dos ToF) |
| `pio device monitor` | Mostra o que a ESP32 escreve na serial (115200) |
| `make -C lib/contrato teste` | Confere o serializador do contrato v1 byte a byte contra `src/contrato/exemplos.jsonl` |
| `make -C simulador-navegacao` | Roda a navegação em todos os labirintos de `simulador-navegacao/labirintos/` ([simulador no PC](simulador-navegacao/README.md)) |

Com a HAL simulada, a ESP32 roda sem nenhum sensor ligado. Depois do health-check o robô **espera o comando `largar`** (a `tel` fica em `health-check` e nenhum `passo` sai); só parte com o último health-check aprovado. O `interromper`, o `health_check` (só parado) e o `largar` chegam pela serial, por exemplo com `python tools/monitor_comandos.py`. O LED azul (GPIO2) mostra o estado do robô (tabela abaixo) e a serial mostra a telemetria do contrato v1, com a `tel` a 5 Hz enquanto o robô se move e a 1 Hz parado, e os eventos (`passo`, `sucesso`, ...) entre elas. O robô simulado **anda de verdade**: os motores, os encoders e os ToF são os do mundo simulado, e cada célula leva cerca de um segundo (anda e para). Compilando com `-D TELEMETRIA_DEPURACAO` sai também, a cada 5 s, uma linha `#` com os descartes de eventos, o maior período do laço da navegação e as distâncias filtradas dos ToF simulados.

### Sem placa: simulador Wokwi

O [Wokwi](https://wokwi.com) simula a ESP32 e roda o mesmo `firmware.bin` que iria para a placa. A configuração já está em `wokwi.toml` e `diagram.json`.

1. No VS Code, instale a extensão **Wokwi Simulator**.
2. `F1` e depois **Wokwi: Request a New License** (abre o navegador; a licença é gratuita, basta criar a conta). Só na primeira vez.
3. Compile com `pio run`.
4. Com a pasta `src/firmware` aberta, `F1` e depois **Wokwi: Start Simulator**.

A saída serial aparece no terminal do VS Code. O Wokwi não simula Bluetooth, mas com a HAL simulada a telemetria sai pela serial. O robô espera o comando `largar` depois do health-check: digite `{"v":1,"cmd":"largar"}` e Enter no terminal serial para ele partir.

### Firmware com a HAL real (`esp32_real_hc`)

Roda o health-check ao ligar e a cada comando `health_check` da ponte, e envia os `hc_item`, o `hc_resultado` e a `tel` (estado `health-check`, a 1 Hz) pelo contrato v1. Não anda: os motores só giram no autoteste, **com as rodas no ar e o interruptor da bateria ligado**. Sem o interruptor, motores e encoders reprovam. O enlace é a serial USB, então a ponte do notebook abre `/dev/ttyACM0`:

```bash
cd src/backend && python -m ponte --porta /dev/ttyACM0
```

O botão **BOOT** passa pela máquina de estados (`lib/estados`) e pelo detector de gestos (`lib/boot`): toque curto (soltar antes de 1,5 s) apaga o mapa da NVS; toque longo (segurar 1,5 s, com bipe duplo) apaga também os tipos resolvidos. A serial mostra, em linhas com `#`, quais chaves (`mem_mapa` e `mem_tipos`) existem na NVS antes e depois de cada toque. O LED azul e o buzzer (GPIO2 e GPIO12) dão o retorno; o buzzer é tratado como ativo. Para ter dados reais na NVS, rode antes o `esp32_sim`, que grava o mapa e os tipos ao percorrer o labirinto.

O DIP lido nas vias 1 e 2 vira o `tipo_dip` (`00` = 4x4, `01` = 8x4, `10` = 12x4, `11` = inválido, que reprova). Com `esp32_real_hc_tof_mock` o ToF não mede nada: ele devolve leituras fixas.

### Verificar a serial

O `tools/verificar_serial.py` lê a serial da ESP32 e diz se alguma mensagem se perdeu, sem precisar da ponte do backend. Para o teste de 10 min (600 s, o padrão), com a placa no USB e o `pio device monitor` fechado:

```bash
cd src/firmware
source .venv/bin/activate                     # o pyserial vem do requirements.txt
python tools/verificar_serial.py --porta /dev/ttyUSB0 --duracao 600
python tools/verificar_serial.py --porta /dev/ttyUSB0 --duracao 600 --largar   # manda o largar 5 s depois de abrir a porta
python tools/verificar_serial.py --arquivo captura.jsonl   # confere linhas já capturadas
python tools/verificar_serial.py --porta /dev/rfcomm0 --duracao 600 --largar   # pelo Bluetooth: o resumo traz o atraso de entrega (p95 até 3 s) e as quedas do enlace (a porta é reaberta sozinha; qualquer queda reprova)
```

O robô espera o comando `largar` depois do health-check. Sem `--largar` a captura só tem o health-check e a `tel` em `health-check`; com `--largar` ela cobre também os `passo` e o `sucesso`.

O `--baud` tem padrão 115200. As linhas que começam com `#` são ignoradas; em cada uma das outras o script confere o tamanho (até 256 bytes contando o `\n`), o JSON, `v = 1`, o tipo e se o `seq` é o anterior mais 1 dentro do mesmo `boot`. No fim imprime o resumo:

- **Linhas por tipo:** quantas `hc_item`, `hc_resultado`, `tel`, `passo`, `falha` e `sucesso` chegaram.
- **Taxa média da tel:** perto de 5 Hz com o robô andando e de 1 Hz parado.
- **Duração:** tempo coberto pelas linhas, medido pelo `t_ms` de cada `boot`.
- **Buracos de seq:** cada `seq` esperado que não veio, com o número recebido no lugar.
- **Descartadas antes da primeira mensagem:** linhas que chegaram ao abrir a porta, antes da primeira mensagem do contrato: o log de boot da ESP32, quando ela reinicia, ou restos de linhas antigas repetidos pelo conversor USB-serial. Elas não contam como inválidas.
- **Linhas inválidas:** número da linha e o motivo.

O script sai com 0 (`Resultado: OK`) só se chegou alguma mensagem do contrato e não houve buraco nem linha inválida; senão sai com 1 (`Resultado: FALHOU`). Os testes dele rodam com `pytest tools`.

### Cobertura dos testes

O ambiente `native_cov` roda os testes do `native` compilados com `--coverage`. O relatório sai pelo [gcovr](https://gcovr.com):

```bash
pip install gcovr
pio test -e native_cov
gcovr -r . --filter lib/ --exclude 'lib/hal/esp32/hal_.*' --print-summary
gcovr -r . --filter lib/ --exclude 'lib/hal/esp32/hal_.*' --html-details cobertura.html
```

Os drivers reais (`lib/hal/esp32/hal_*`) ficam de fora porque só compilam na placa. O `src/` (tarefas do FreeRTOS) também, porque não entra no `native`. A meta do 7.4 é de pelo menos 80% das linhas.

### Guia de comandos: o que rodar em cada situação

Rode a partir de `src/firmware`, com a ESP32 no USB (`/dev/ttyACM0`). Os motores só giram com o **interruptor da bateria ligado**. Se o monitor abrir mudo depois de um upload, aperte o **EN** (reset).

**1. Testes dentro da ESP32** (não movem nada; esperado: todos `PASSED`):

| Comando | O que prova |
|---|---|
| `pio test -e esp32_sim -f test_estados --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0` | máquina de estados (28 casos) |
| `... -f test_boot` | gestos do botão BOOT (20) |
| `... -f test_corrida` | corrida: `largar`, `interromper`, retomada, BOOT, NVS da flash (19) |
| `... -f test_sinal` | padrão do LED por estado (9) |
| `... -f test_encoder_soma` | soma do encoder sem interrupção (6) |
| `... -f test_execucao` | executor de movimento, andando no mundo simulado (11) |
| `... -f test_desempenho_nav` | flood fill abaixo de 5 ms |

Os testes `test_corrida` e `test_execucao` apagam `mem_mapa` e `mem_tipos` na flash. Não rode em laço.

**2. Corrida simulada na placa** (HAL simulada, o robô anda pelo mundo simulado):

| Comando | Para quê |
|---|---|
| `pio run -e esp32_sim -t upload` | corrida simulada sem botão físico |
| `pio run -e esp32_sim_boot -t upload` | o mesmo, com o BOOT físico: aperte o botão no meio da corrida e veja a `falha` com origem `boot` |
| `python tools/monitor_comandos.py` | monitor com teclas: **`l`** largar, **`i`** interromper, **`h`** health_check, **`t`** mostra toda a `tel`, **`q`** sai. O robô só parte depois do `l` |

**3. Bancadas** (programas interativos, não são o firmware do robô). Abra com `pio device monitor --port /dev/ttyACM0 --baud 115200` e digite a tecla:

| Ambiente | Teclas |
|---|---|
| `esp32_drivers` | **`e`** / **`d`** motor esquerdo / direito para a frente, **`E`** / **`D`** para trás, **`p`** para, **`n`** grava e lê um contador na NVS. Imprime `enc_e`, `enc_d` e a bateria a cada 200 ms; cada tecla é repetida como `# comando: x`; qualquer outra tecla para, e os motores param sozinhos depois de 20 s. Serve para ver o sentido das rodas, os pulsos e o estouro do contador |
| `esp32_hc_bancada` | **`d`** DIP, **`b`** bateria, **`t`** 40 leituras dos ToF, **`r`** reinicia os 3 ToF sem reiniciar a placa, **`m`** autoteste dos motores (rodas no ar), **`h`** health-check completo, **`i`** varredura do I²C, **`x`**, **`f`** e **`v`** diagnóstico do XSHUT e do sensor, **`c`** e **`o`** clock e timeout do I²C |
| `esp32_calibracao` | ver [Calibração no robô](#calibração-no-robô): **`a`** **`A`** **`g`** **`G`** **`h`** **`Q`** **`v`** **`s`** **`t`** **`p`**, cada uma seguida de Enter |

**4. Firmware do robô** (HAL real; o robô espera o `largar` depois do health-check, igual à simulação):

| Ambiente | Quando usar |
|---|---|
| `esp32_real_hc` | só health-check, telemetria e BOOT (não anda). Reprova enquanto os ToF laterais não funcionarem |
| `esp32_real_hc_tof_mock` | o mesmo com ToF mockado: o health-check aprova |
| `esp32_real` | firmware completo, **anda**. Só depois da calibração, com o robô numa pista. Comece com as rodas no ar |
| `esp32_real_tof_mock` | o completo com ToF mockado, para andar com os ToF laterais em falta |

Teclas e comportamentos comuns ao `esp32_real_hc` e ao `esp32_real` (por `tools/monitor_comandos.py` ou pelo JSON da ponte):
- **`l` (`largar`)**: parte, só com o último health-check aprovado.
- **`i` (`interromper`)**: na corrida para os motores e envia a `falha` com origem `web` e motivo nulo.
- **`h` (`health_check`)**: só vale parado; roda os 8 itens de novo e relê o DIP.
- **BOOT, toque** na corrida: para e envia a `falha` com origem `boot`. **Curto** parado (menos de 1,5 s): apaga o mapa da NVS (1 bipe). **Longo** (1,5 s ou mais): apaga também os tipos resolvidos (2 bipes). Cada apagamento imprime uma linha `# BOOT curto: ...` ou `# BOOT longo: ...` no monitor. Na HAL simulada (`esp32_sim_boot`) não há buzzer, então o sinal é só essa linha e o LED; o efeito aparece no `inicio` do próximo boot (`nova`) e na `tel` voltando para (0,0). O BOOT só age com o robô parado fora do health-check (esperando o `largar`, em `success` ou em `failed`).

**5. Verificar a telemetria de 10 min:** `python tools/verificar_serial.py --porta /dev/ttyACM0 --duracao 600 --largar` (ver [Verificar a serial](#verificar-a-serial)).

### Como o robô anda (FIRM-10)

O laço da `tarefa_navegacao` roda a cada `MOV_PERIODO_MS` (5 ms), o ciclo do controle de movimento. O robô **anda e para**:
1. Parado no centro de uma célula, espera 200 ms para a mediana dos ToF estabilizar.
2. Lê a parede da frente (o frontal, parado no centro, lê com confiança) e junta com a **parede lateral guardada ao entrar na célula**; pede a decisão à navegação.
3. A `lib/execucao` executa a ação: `FRENTE` avança uma célula; `DIREITA` gira -90° e avança; `ESQUERDA` gira +90° e avança; `MEIA_VOLTA` gira 180° e avança.
4. Durante o avanço, quando o robô já andou `EXEC_LEITURA_LATERAL_MM` (100 mm, ~10 mm depois da fronteira), guarda a parede lateral da célula nova. **Os ToF laterais a 45° só leem a célula com confiança logo ao entrar nela:** no centro do lado aberto eles enxergam o poste do canto e dão parede falsa.
5. Se uma roda trava ou o movimento passa de `MOV_TEMPO_LIMITE_MS`, a corrida vai a `failed`, grava na NVS e envia a falha automática `stuck`.

Na simulação isso roda igual, pela física do mundo simulado. O PID de velocidade por roda (`vel_passo`) é validado na bancada de calibração (tecla `v`); o movimento segue com controle de posição e alimentação direta.

A NVS é gravada a cada célula nova, e no máximo `MEM_GRAVACOES_MAX` (600) vezes por inicialização, para um robô que oscile entre células não gastar a flash. Só a gravação do fim da corrida e da falha continua depois do limite.

### Bluetooth real (FIRM-11)

Na pista o robô anda sem o cabo USB, então a telemetria e os comandos vão pelo Bluetooth clássico (SPP). O driver é `lib/hal/esp32/hal_bt_esp32.cpp` (BluetoothSerial, nome **Ratatouille**), ligado só com `-D BT_REAL`; sem ele continua valendo o enlace pela serial USB (`hal_bt_serial_esp32.cpp`). Os ambientes com Bluetooth usam a partição `huge_app`, porque o app do Bluetooth clássico passa de 1 MB.

| Ambiente | Para quê |
|---|---|
| `esp32_real_bt` | firmware completo; a ponte do backend lê o contrato pelo `/dev/rfcomm0` |
| `esp32_real_bt_tof_mock` | o mesmo com ToF mockado |
| `esp32_calibracao_bt` | calibração: as teclas funcionam pelo USB **e** pelo Bluetooth |

Pareamento no notebook (Linux). A gravação é sempre pelo USB; depois você tira o cabo:
```bash
bluetoothctl
  power on
  agent on
  scan on                      # espere aparecer "Ratatouille" e anote o endereço
  pair  XX:XX:XX:XX:XX:XX
  trust XX:XX:XX:XX:XX:XX
  exit
sudo rfcomm bind /dev/rfcomm0 XX:XX:XX:XX:XX:XX 1     # cria a porta; a conexão abre quando um programa a abre
```
O endereço do Bluetooth é o da base (impresso no upload, `MAC:`) mais 2 no último byte: com `88:57:21:70:7f:0c` fica `88:57:21:70:7f:0e`. Para liberar a porta: `sudo rfcomm release /dev/rfcomm0`. Não use `rfcomm connect` junto com o monitor: o `connect` segura a porta para ele mesmo (`tty-attached`) e o monitor recebe `Device or resource busy`. Use o `bind` e abra a porta com o monitor, que faz a conexão.

**Atenção ao eco do terminal:** o `/dev/rfcomm0` devolve ao robô tudo o que recebe dele, enquanto estiver em modo de terminal comum. Por isso a bancada de calibração só aceita **linhas com uma tecla só** (digite a tecla e Enter) e ignora o resto, e o firmware do robô só aceita os comandos JSON do contrato. Se ainda assim quiser desligar o eco, use `sudo stty -F /dev/rfcomm0 raw -echo` depois do `rfcomm connect`.

**Notebook Linux: ignorar o `rfcomm` no ModemManager.** O ModemManager abre o `/dev/rfcomm0` assim que a porta nasce, sonda por cerca de 20 s e toma as linhas da telemetria: o programa que lê a porta (monitor, verificador ou ponte) perde uns 20 segundos de mensagens no começo de cada conexão e recebe uma linha partida. Teste 5.36 do 7.5. Para ele ignorar essas portas:
```bash
echo 'KERNEL=="rfcomm*", ENV{ID_MM_DEVICE_IGNORE}="1"' | sudo tee /etc/udev/rules.d/99-rfcomm-ignorar-modemmanager.rules
sudo udevadm control --reload-rules
sudo systemctl restart ModemManager
```
Parar o serviço (`sudo systemctl stop ModemManager`) também resolve, mas só até o próximo reinício e desliga o gerenciamento de modems móveis. Se o notebook não usa modem 3G ou 4G, tanto faz.

**Alcance:** a mais de cerca de 3 m do notebook o enlace desconecta com facilidade (teste 5.27 do 7.5, ainda sem medição nem causa identificada). Fique a menos de 3 m; a ponte reabre a porta sozinha se a conexão cair.

Usar o robô pelo Bluetooth, no lugar do USB (os comandos são os mesmos do [guia](#guia-de-comandos-o-que-rodar-em-cada-situação)):
```bash
python tools/monitor_comandos.py /dev/rfcomm0           # teclas l, i, h
pio device monitor --port /dev/rfcomm0 --baud 115200    # bancada de calibração: as teclas a, A, g, G, ...
python tools/verificar_serial.py --porta /dev/rfcomm0 --duracao 600 --largar
python -m ponte --porta /dev/rfcomm0                    # ponte do backend (src/backend)
```
No Windows a porta é a `COMx` que o pareamento cria. Se a conexão cair, a ponte reabre sozinha; com o `rfcomm`, abra a porta de novo.

### Calibração no robô

Grave `esp32_calibracao` (USB) ou `esp32_calibracao_bt` (USB e Bluetooth), ligue o interruptor da bateria e abra o monitor serial, ou, para calibrar na pista **sem cabo**, grave a versão `_bt`, tire o USB, pareie e abra o `/dev/rfcomm0` ([Bluetooth real](#bluetooth-real-firm-11)). Cada tecla imprime o que o robô mediu, para você comparar com a régua, o transferidor e o multímetro. A bancada não grava nada na NVS. Um Enter sozinho (linha vazia) ou um `p` para os motores. Para testar o movimento, o robô precisa estar **no chão**, com espaço livre à frente: o `A` usa 1,8 m.

| Tecla | O que faz | O que conferir | O que ajustar em `config/robo.h` |
|---|---|---|---|
| `a` | avança 1 célula, com a parada de emergência pelo ToF frontal ligada (lê o ToF a cada 50 ms, como o firmware completo) | imprime os pulsos de cada roda (esperado 1079), a distância pelos pulsos e a leitura frontal ao parar; meça com a régua (180 mm). O avanço para com `FALHOU` a tempo de sobrar 17 mm até a parede da frente (`MOV_FRENTE_SOBRA_MM`), contando o que o robô ainda anda até parar. **Rode primeiro, com o robô seguro**. Se der `FALHOU` com os pulsos poucas unidades abaixo de 1079, a roda parou antes do fim por causa da zona morta do motor | `MOV_KI_POSICAO` (a parcela integral que vence o atrito no fim; suba se a roda ainda parar antes) e `MOV_KP_POSICAO` |
| `A` | 10 avanços seguidos, com 0,5 s de pausa | meça a distância total na pista: esperado 1800 mm, erro final de até 10 mm | `ENCODER_PULSOS_POR_VOLTA` e `RODA_DIAMETRO_MM`, que definem `MM_POR_PULSO` |
| `g` / `G` | gira 90° à esquerda / à direita | imprime os pulsos de cada roda (esperado 435); confira o ângulo a olho | |
| `h` | gira 180° | pulsos esperados 871 | |
| `Q` | 10 giros de 90° à esquerda, seguidos | o robô termina virado 180° em relação à partida (2,5 voltas); meça o desvio com o transferidor: até 5° | `BITOLA_MM` |
| `v` | as duas rodas a 400 mm/s por 3 s, até 1,2 m de percurso: **só com as rodas no ar**, nunca no chão | imprime a velocidade medida e, no fim, a média depois da rampa e o erro: `APROVADO` com até 5% nas duas | ganhos do PI em `lib/controle/velocidade.c` |
| `s` | gira o motor esquerdo a 300 mm/s por até 8 s | segure a roda: imprime em quanto tempo a trava foi detectada | `MOTOR_TRAVADO_MS`, `MOV_TRAVADO_MS` e `MOV_TRAVADO_POTENCIA` |
| `t` | 40 leituras dos ToF (a 20 Hz), com as paredes decididas `F E D` | ponha o robô no centro de células com e sem parede e anote as distâncias | `PAREDE_FRENTE_*_MM` e `PAREDE_LADO_*_MM` |
| `p` | para os motores | | |

Os outros critérios da FIRM-10 não têm tecla própria:
- **Corredor de 4 células sem encostar:** a centralização pelos ToF laterais está ligada (`MOV_KP_CENTRO` 0,05) e só age com **as duas paredes** lidas; com uma parede só ela fica desligada (`MOV_CENTRALIZAR_UMA_PAREDE` 0), porque `MOV_LADO_CENTRADO_MM` não foi conferido na pista. No simulador, com uma roda andando 3% a mais que os pulsos, o robô atravessa o corredor de 4 células sem encostar (`test_movimento`); falta repetir na pista com os dois ToF laterais funcionando. `MOV_GANHO_FRONTAL` segue 0 (desligado).
- **Erro que não se soma:** o que faltou ou passou do alvo em um avanço ou giro entra no movimento seguinte (`residuo_avanco` e `residuo_giro` em `movimento.c`), então 10 avanços terminam a até 10 mm e 10 giros de 90° a até 5° (testes `test_dez_avancos_seguidos_erram_ate_10_mm` e `test_dez_giros_de_90_graus_desviam_ate_5_graus`, no simulador).
- **Bateria com erro de até 2%:** na bancada `esp32_drivers`, compare a tensão impressa com o multímetro e ajuste `BATERIA_DIVISOR` e `BATERIA_QUEDA_DIODO_MV`.

Os valores finais vão para `config/robo.h` e para o roteiro de testes de integração (7.5).

### LED por estado

O LED da placa (GPIO2) segue a máquina de estados (`lib/sinal`):

| Estado | LED |
|---|---|
| health-check rodando | pisca rápido (400 ms, 200 aceso) |
| aguardando o `largar` | aceso fixo |
| reprovado | pisca lento (2 s, 1 s aceso) |
| running | pisca médio (1 s, 500 ms aceso) |
| success | 3 piscadas e fica aceso |
| failed | pisca muito rápido (200 ms, 100 aceso) |

O bipe do BOOT (1 toque apaga o mapa, 2 apagam tudo) continua pelo buzzer e pisca o LED junto.

### Rodar os testes dentro da ESP32

Os testes `test_estados`, `test_boot`, `test_corrida` e `test_sinal` têm um `entrada_arduino.cpp` e rodam também na placa, com a ESP32 ligada no USB:

```bash
pio test -e esp32_sim -f test_estados --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
pio test -e esp32_sim -f test_boot --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
pio test -e esp32_sim -f test_corrida --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
pio test -e esp32_sim -f test_sinal --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
```

O `test_corrida` usa a NVS da flash: apaga `mem_mapa` e `mem_tipos` antes de cada caso e deixa o mapa do último gravado, então o primeiro boot seguinte do firmware sai como `retomada`.

### Medir o desempenho da navegação na placa

O flood fill precisa recalcular em menos de 5 ms na ESP32. O teste `test_desempenho_nav` monta o pior caso (12x4 com a orientação indefinida e paredes em serpentina) e mede `nav_recalcular` 100 vezes. Com a ESP32 ligada no USB:

```bash
pio test -e esp32_sim -f test_desempenho_nav
```

Ele grava o teste, abre a serial e mostra uma linha `nav_recalcular em 100 execucoes: media ... us, maximo ... us`. O teste passa (`PASSED`) se o máximo ficar abaixo de 5000 µs; anote a média e o máximo no PR ou na issue. No PC, o mesmo teste roda dentro de `pio test -e native`.

## Organização

```text
src/firmware/
├── platformio.ini        ambientes: esp32_sim (ESP32) e native (PC/testes)
├── include/config/       pinos.h e robo.h (valores e fontes do ARQ-09)
├── lib/
│   ├── hal/              contratos do hardware (hal_tof.h, hal_motor.h, ...)
│   │   ├── sim/          implementação simulada de cada contrato
│   │   └── esp32/        drivers reais (bateria, motores e encoders); compilados sem HAL_SIMULADA
│   ├── simulacao/        mundo simulado: labirinto em texto + física do robô
│   ├── simulador/        corrida célula a célula e roteiro de telemetria (FIRM-02)
│   ├── contrato/         serializador do contrato v1 (ARQ-01) e seu teste byte a byte
│   ├── telemetria/       buffer de 64 eventos, taxas da tel, interromper e boot na NVS (FIRM-07)
│   ├── telemetria_nav/   conversão do rumo e das paredes da navegação para o contrato
│   ├── sensores/         ToF: mediana, paredes com histerese, autoteste e sequência XSHUT (FIRM-03)
│   ├── energia/          estado da bateria
│   ├── memoria/          mapa e estado na NVS com versão e CRC, para retomar depois de um reset (FIRM-08)
│   ├── estados/          máquina de estados do robô: health-check, aguardando, reprovado, running, success e failed (FIRM-09), C puro
│   ├── boot/             gestos do botão BOOT com debounce: toque, curto e longo (FIRM-09), C puro
│   ├── health_check/     os 8 itens do health-check e o autoteste de motor e encoder (FIRM-09)
│   ├── dip/              tipo do labirinto a partir das vias 1 e 2 do DIP (FIRM-09)
│   ├── sinal/            padrão do LED para cada estado do robô (FIRM-09), C puro
│   ├── execucao/         executa a ação da navegação com as primitivas de movimento: giro e avanço, e a hora de ler a parede lateral (FIRM-10), C puro
│   ├── corrida/          junta a máquina, a navegação e a NVS: largar, interromper, health_check e BOOT (FIRM-09), C puro
│   ├── controle/         PID e movimentos (FIRM-04, FIRM-05)
│   └── navegacao/        flood fill (FIRM-01), C puro
├── src/
│   ├── main.cpp          setup() e loop() do Arduino
│   └── app/              tarefas FreeRTOS: navegação (traduz botão e comandos em eventos da corrida) e telemetria
├── simulador-navegacao/  programa de PC que roda a navegação nos labirintos (FIRM-02)
│   └── labirintos/       24 labirintos válidos: 4x4, 8x4 e 12x4, espelhados
├── tools/                verificar_serial.py (confere a telemetria da serial) e monitor_comandos.py (comandos por tecla)
└── test/                 testes Unity que rodam no PC
```

As camadas só dependem das de baixo: `app` usa `navegacao`, `controle` e `sensores`, que usam a `hal`. A `navegacao/` não pode incluir Arduino, FreeRTOS nem a HAL (o CI confere); ela recebe as paredes como parâmetro e devolve o próximo movimento. Assim o mesmo código roda na ESP32, nos testes e no simulador do PC (FIRM-02).

### HAL real e HAL simulada

Cada arquivo da `hal/` é um contrato: diz **o que** o firmware pode pedir ao hardware. A implementação simulada (`hal/sim/`, compilada com `-D HAL_SIMULADA`) responde a esses contratos a partir do mundo simulado: os ToF medem a distância até as paredes do labirinto, os encoders contam o quanto cada roda andou, a bateria descarrega e as rodas travam se o robô bater. Os drivers reais ficam em `hal/esp32/` (bateria, motores, encoders, NVS, DIP, botão BOOT, LED e buzzer, e os ToF) e são compilados no CI pelos programas de bancada e pelo firmware completo `esp32_real` (FIRM-10, #141). Trocar um pelo outro não muda o resto do código: a lógica acima da HAL (por exemplo, a `lib/sensores/` do FIRM-03) é a mesma nos dois casos.

### Tarefas

| Tarefa | Núcleo | Função |
|---|---|---|
| `navegacao` | 1 | Laço de 5 ms: roda o controle de movimento, lê os ToF a 20 Hz, decide parado no centro da célula, executa o giro e o avanço, trata os comandos e o BOOT e publica a amostra (5 Hz) e os eventos do contrato nas filas; nunca espera pela telemetria |
| `telemetria` | 0 | Envia o contrato v1 pelo Bluetooth (`tel` a 5 Hz em movimento e 1 Hz parado), guarda os últimos 64 eventos para reenviar depois de uma queda e lê o comando `interromper` vindo da ponte |

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

A simulação lê labirintos desenhados assim. No [simulador do PC](simulador-navegacao/README.md), a largada é a célula marcada com `L` (sem a marca, a do canto inferior esquerdo) e o robô começa virado para a saída dela; o mundo simulado da ESP32 sempre começa em (0, 0), virado para o norte:

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

- **Pinos:** `config/pinos.h` segue a folha de Controle do esquemático (ARQ-09). O esquemático ainda usa as 5 redes do A4988, mas a TB6612 pede 7 sinais: os dois `IN2` foram para os GPIO livres 15 e 18 (o 18 era o XSHUT do segundo frontal) e precisam de fio na placa. A decisão é da Eletrônica, que também precisa trocar o pull-up de `MOT_EN` (R1) por pull-down, para a TB6612 não ligar os motores no boot.
- Ainda provisórios em `config/robo.h`: pulsos por volta do encoder e posição x/y dos ToF (Estrutura), as curvas de descarga da bateria (Energia, até os testes da 7.2), os limiares de parede dos ToF (`PAREDE_*_MM`), `EXEC_LEITURA_LATERAL_MM` e os ganhos de centralização (`MOV_KP_CENTRO`, `MOV_GANHO_FRONTAL`, desligados): calibrar na pista com a bancada `esp32_calibracao`.
- **ToF laterais:** não respondem por defeito no conector da placa (teste 5.2 do 7.5); a centralização e o corredor de 4 células dependem do conserto.
- **Aviso, defeito estrutural:** a bateria fica entre a roda boba e a roda esquerda e atrapalha o movimento (atrito que varia): nos testes ela fez avanços falharem (por exemplo, o avanço 4 do `A`) e a esquerda terminar curta ou passada. O controle não foi adaptado a isso; o reparo é mecânico (fixar a bateria longe das rodas). Os resultados de avanço e giro devem ser lidos com essa ressalva (testes 5.28 a 5.35 do 7.5).
- **Não verificado na placa:** a tecla `s` (limiar de stuck): a roda segurada sem força não parou o encoder, então o detector não disparou. O stuck no movimento segue coberto pelos testes no simulador.
- **A validar no labirinto real:** a parada de emergência pelo ToF frontal (`MOV_FRENTE_SOBRA_MM` 17 mm e `MOV_TOF_ATRASO_S`; a primeira versão falhou na pista), os limiares `PAREDE_*_MM` e `MOV_FRENTE_ALVO_MM`, e os testes no chão (`a`, `A`, `g`, `G`, `h`, `Q`).
- **Bancada (ARQ-09):** montar ESP32 + ToF + motor com encoder + ponte H quando os componentes chegarem; é nela que se confirmam os valores provisórios acima.

> [!WARNING]
> **Não acrescente arquivos referentes a _hardware_ nesta pasta.** Eles deverão ser armazenados na pasta [hw](../../hw) deste repositório. Também não versione a saída da compilação (`.pio/`, `.bin`, `.elf`).
