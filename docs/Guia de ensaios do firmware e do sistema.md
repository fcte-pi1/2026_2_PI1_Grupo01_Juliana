# Guia de ensaios do firmware e do sistema

Este guia reúne, na ordem em que foram usados no projeto, os comandos para testar o **firmware** na ESP32 e o **sistema completo** (robô, ponte, back-end e tela). Serve para quem quer repetir os ensaios do zero. Cada etapa diz o que precisa estar ligado e o que ela valida.

Os detalhes de cada ambiente estão no [README do firmware](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/tree/dev/src/firmware#comandos). Os resultados registrados ficam em [Testes de integração (7.5)](7.5%20-%20Testes%20de%20integra%C3%A7%C3%A3o.md).

!!! warning "O que vale como evidência"
    Resultados com **ToF mockado** (`*_tof_mock`) ou com as **rodas no ar** validam só o fluxo da tela, da ponte e do back-end. Eles não validam a navegação, porque os sensores são fixos ou não há deslocamento. Para o FIRM-12 e o FIRM-13 só vale a corrida na pista, com os três ToF reais.

## 1. Antes de começar

| Item | Para quê | Como conferir |
|---|---|---|
| PlatformIO (venv, versão 6.2.0) | compilar e gravar o firmware | `cd src/firmware && source .venv/bin/activate && pio --version` |
| Grupo `dialout` | gravar na placa pelo USB (Linux) | `groups` deve listar `dialout` |
| Docker com `compose` | banco de dados | `docker compose version` |
| Python 3.11 ou mais novo e Node 20 ou mais novo | back-end, ponte e tela | `python3 --version` e `node --version` |
| Bluetooth do notebook | só nos ensaios sem cabo | `bluetoothctl show` mostra `Powered: yes` |

Nos exemplos a placa está em `/dev/ttyACM0`. Se a sua for outra, troque o valor de `--upload-port` e de `--port`. O endereço do Bluetooth do robô nos exemplos é `88:57:21:70:7F:0E`; o seu aparece no `bluetoothctl devices` depois do pareamento.

## 2. Mapa dos ensaios

| Ensaio | Ambiente | Precisa de | Valida |
|---|---|---|---|
| Testes no PC | `native` | só o notebook | lógica do firmware: navegação, máquina de estados, contrato, telemetria |
| LED piscando | `ele09_pisca` | placa | gravação e porta serial |
| Navegação na placa | `esp32_sim` | placa | o mesmo firmware com a HAL simulada, sem sensores |
| Corrida simulada com BOOT | `esp32_sim_boot` | placa | comandos, LED, BOOT físico e memória na NVS |
| Componentes um a um | `esp32_hc_bancada`, `esp32_drivers` | robô montado | DIP, bateria, ToF, motores, encoders e health-check |
| Firmware real por USB | `esp32_real`, `esp32_real_tof_mock` | robô montado | health-check e corrida com a HAL real |
| Calibração | `esp32_calibracao`, `esp32_calibracao_bt` | robô na pista | pulsos por célula e por giro, velocidade, stuck, limiares |
| Sistema completo | `esp32_real_bt`, `esp32_real_bt_tof_mock` | robô, Bluetooth e notebook | tela, ponte, back-end e robô juntos |

## 3. Testes no PC, sem placa

```bash
cd src/firmware
pio test -e native                    # todos os testes Unity
pio test -e native_cov                # os mesmos, com dados de cobertura
make -C lib/contrato teste            # serializador do contrato, byte a byte
```

```bash
cd src/backend
TEST_DATABASE_URL=postgresql+psycopg://micromouse:micromouse@localhost:5433/micromouse_test \
  .venv/bin/python -m pytest -q       # back-end e ponte
```

```bash
cd src/frontend
npx tsc -b && npx eslint src && npx vitest run    # tela
```

## 4. Só a placa, sem o robô montado

**Gravar e testar o LED:**

```bash
cd src/firmware
pio run -e ele09_pisca -t upload --upload-port /dev/ttyACM0
```

**Apagar a flash inteira**, inclusive o mapa guardado na NVS, quando a placa se comporta de forma estranha:

```bash
pio run -t erase --upload-port /dev/ttyACM0
```

**Navegação com a HAL simulada**, com a telemetria no monitor:

```bash
pio run -e esp32_sim -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

O ambiente `esp32_sim_depuracao` é igual, mas imprime a cada 5 s o maior período do laço de navegação e as distâncias dos ToF simulados.

**Teste de desempenho da navegação dentro da ESP32:**

```bash
pio test -e esp32_sim -f test_desempenho_nav --upload-port /dev/ttyACM0 --test-port /dev/ttyACM0
```

**Corrida simulada com comandos por tecla e o botão BOOT:**

```bash
pio run -e esp32_sim_boot -t upload --upload-port /dev/ttyACM0
python tools/monitor_comandos.py
```

Teclas do monitor: `h` health-check, `l` largar, `i` interromper, `t` mostra toda a `tel`, `q` sai. O robô liga **ocioso**, não roda o health-check sozinho e só parte com o `largar` depois de um health-check aprovado.

Para digitar a linha JSON à mão no monitor do PlatformIO, o terminador precisa ser LF; sem `--eol LF` o firmware ignora a linha:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200 --filter send_on_enter --eol LF --echo
```

**Memória na NVS (retomada depois do reset).** Com o `esp32_sim_boot`, interrompa a corrida, aperte o botão EN da placa e confira que a navegação volta da mesma célula. Para zerar a memória: toque longo no BOOT (1,5 s ou mais, dois bipes) ou `pio run -e esp32_sim_boot -t erase`.

## 5. Robô montado, por cabo USB

Com o interruptor da bateria ligado, as rodas **no ar** nos testes de bancada e **no chão** nos de corrida.

**Componentes um a um** (DIP, bateria, ToF, motores, health-check):

```bash
pio run -e esp32_hc_bancada -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

**Drivers dos motores, pulsos por volta e sentido das rodas** (motores parados ou no ar):

```bash
pio run -e esp32_drivers -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

**Health-check na placa real**, com a HAL real e os ToF reais:

```bash
pio run -e esp32_real -t upload --upload-port /dev/ttyACM0
python tools/monitor_comandos.py
```

No boot do `esp32_real` as três linhas `E (...) pcnt: ...` não devem aparecer. Se aparecerem, o contador de pulsos está com problema.

Com os ToF laterais em falta, use `esp32_real_tof_mock`: o health-check aprova com as leituras fixas (frontal 300 mm, laterais 120 mm), só para exercitar o fluxo.

## 6. Bluetooth

A gravação é sempre pelo USB. Depois você tira o cabo.

**Parear (uma vez):**

```bash
bluetoothctl
  power on
  agent on
  default-agent
  scan on                       # espere aparecer "Ratatouille" e anote o endereço
  pair  88:57:21:70:7F:0E       # se pedir PIN, tente yes ou 1234
  trust 88:57:21:70:7F:0E
  scan off
  exit
```

**Criar a porta e abrir:** use `bind`, não `connect`. O `connect` segura a porta para ele mesmo e o monitor ou a ponte recebem `Device or resource busy`.

```bash
sudo rfcomm bind /dev/rfcomm0 88:57:21:70:7F:0E 1
pio device monitor --port /dev/rfcomm0 --baud 115200 --echo
```

A conexão só abre quando um programa abre a porta (o monitor, o `monitor_comandos.py` ou a ponte).

**Soltar e limpar tudo** do Bluetooth do robô:

```bash
sudo rfcomm release all
bluetoothctl disconnect 88:57:21:70:7F:0E
bluetoothctl remove 88:57:21:70:7F:0E      # esquece o pareamento: pareie de novo depois
```

**ModemManager.** Ele abre o `/dev/rfcomm0` assim que a porta nasce e toma as primeiras linhas. Para ele ignorar as portas `rfcomm` (uma vez):

```bash
echo 'KERNEL=="rfcomm*", ENV{ID_MM_DEVICE_IGNORE}="1"' | sudo tee /etc/udev/rules.d/99-rfcomm-ignorar-modemmanager.rules
sudo udevadm control --reload-rules
sudo systemctl restart ModemManager
```

**Calibração pela pista, sem cabo** (as mesmas teclas pelo USB e pelo Bluetooth):

```bash
pio run -e esp32_calibracao_bt -t upload --upload-port /dev/ttyACM0
sudo rfcomm bind /dev/rfcomm0 88:57:21:70:7F:0E 1
pio device monitor --port /dev/rfcomm0 --baud 115200 --echo
```

O alcance fica bom até cerca de 3 m do notebook.

## 7. Sistema completo pela tela

O objetivo é operar e coletar os dados **só pela tela**, sem comando de terminal durante o ensaio. O terminal serve para gravar o firmware e subir o ambiente.

**Gravar o firmware com o Bluetooth real** (robô no USB, depois desconecte o cabo):

```bash
cd src/firmware
pio run -e esp32_real_bt -t upload --upload-port /dev/ttyACM0         # sensores reais
pio run -e esp32_real_bt_tof_mock -t upload --upload-port /dev/ttyACM0  # ToF mockado, para testar o fluxo
```

Só grave o `esp32_real_bt_tof_mock` para testar o fluxo. Para a evidência, volte ao `esp32_real_bt`.

**Subir banco, back-end, tela e ponte** com um comando, na raiz do repositório:

```bash
scripts/ambiente.sh                                          # banco, API e tela, sem robô
scripts/ambiente.sh subir --ponte --bluetooth 88:57:21:70:7F:0E   # com o robô
scripts/ambiente.sh subir --instalar-sistema                 # ambiente novo (Ubuntu/Debian): instala o que falta
```

O script confere Docker, Python e Node, instala só o que falta, sobe o Postgres, migra o banco, sobe a API (porta 8000) e a tela sem mock (porta 5173), faz o `rfcomm bind` com `sudo` e liga a ponte. Quando aparecer "Tudo no ar", abra `http://127.0.0.1:5173`.

```bash
scripts/ambiente.sh status           # o que está no ar
scripts/ambiente.sh logs ponte       # ou api, ou front
scripts/ambiente.sh parar            # para tela, API e ponte que o script subiu
scripts/ambiente.sh parar --tudo     # para também o banco
```

**Antes de largar:**

- O robô sobe parado. A largada exige um health-check aprovado nos últimos 10 minutos, com o DIP igual ao labirinto escolhido.
- Os motores só andam com o interruptor da bateria ligado, e o robô precisa estar no chão, na pista.
- A bateria deve estar acima de 7000 mV na leitura. Abaixo disso o health-check reprova a bateria; a tela deixa continuar mesmo assim, com um aviso, mas isso não substitui carregar a LiPo.
- Para o primeiro ensaio num labirinto novo, apague a memória do robô: toque longo no BOOT, com o robô parado.

**Roteiro na tela:** peça o health-check (8 de 8 OK), escolha o labirinto, preencha a configuração e a observação, abra a execução, acompanhe a corrida e confira o Histórico, o número da execução e a exportação CSV ou JSON. O roteiro completo dos casos (DIP divergente, validade, encerrar, três tentativas) está em [Testes de integração](7.5%20-%20Testes%20de%20integra%C3%A7%C3%A3o.md#roteiro-e2e).

**Cuidados no ensaio:**

- Rótulo de cada corrida: use a configuração para separar `mock-rodas-no-ar` de `labirinto-real-4x4`. O Histórico filtra por configuração.
- Compare o trajeto da tela com o que o robô fez de fato (régua ou vídeo): divergência indica encoder ou deslize.

## 8. LED da placa

| LED | Estado |
|---|---|
| batida breve de 100 ms a cada 3 s | ocioso, ligado e sem health-check |
| pisca rápido (0,4 s) | health-check rodando |
| aceso fixo | health-check aprovado, esperando o `largar` |
| pisca lento (2 s) | health-check reprovado |
| pisca médio (1 s) | corrida em andamento |
| 3 piscadas e fica aceso | `success` |
| pisca muito rápido (0,2 s) | `failed`, até um novo comando |

Para sair de `failed`: peça um novo health-check pela tela, ou retome a tentativa. O botão EN reinicia a placa e perde o estado da corrida.

## 9. Problemas comuns

| Sintoma | Causa provável | O que fazer |
|---|---|---|
| Itens do health-check "pendente" e linhas quebradas no log da ponte (`{v":,bo"4,...`) | duas pontes lendo o mesmo `/dev/rfcomm0` | `ps -eo pid,cmd \| grep "ponte --porta"` e `kill` na que sobrar. O `scripts/ambiente.sh` já recusa subir a segunda |
| O script fica parado em "Back-end" | uma API antiga travada ocupa a porta 8000 | o script agora avisa o PID; `kill <PID>` (ou `kill -9`) e rode de novo |
| Tela em branco | o Vite guardou um módulo vazio ao gravar um arquivo | `touch` no arquivo, ou pare e suba o Vite de novo |
| Receptor em "Ponte desligada" | a ponte não está relatando | `scripts/ambiente.sh subir --ponte --bluetooth <MAC>`; uma ponte antiga, sem o relato do enlace, também mostra assim |
| Receptor em "Porta aberta" sem sinal | o robô parado não envia nada | normal; peça um health-check ou clique em **Reconectar** |
| Bluetooth não conecta, `Connected: no` e queda a cada 5 s | robô sem energia ou travado | carregue a LiPo, aperte EN, confirme que nada mais está conectado ao SPP, depois **Reconectar** ou refaça o `bind` |
| Bateria reprovada, por exemplo 6861 mV | LiPo descarregada | carregar até perto de 8,4 V; evite continuar abaixo de 6700 mV, em que o firmware passa a ler a fonte como pilhas AA |
| O robô parece "já saber" as paredes | mapa antigo na NVS | toque longo no BOOT com o robô parado, antes de repetir o ensaio |
| `Device or resource busy` ao abrir `/dev/rfcomm0` | `rfcomm connect` aberto, ou outro programa na porta | use `bind`; `sudo rfcomm release all` |
| `pio` não encontrado ou erro de `resultcallback` | PlatformIO instalado pelo `apt` | remova o pacote e use o venv do README do firmware |

## 10. O que cada resultado prova

| Ensaio | Prova | Não prova |
|---|---|---|
| Testes no PC | lógica e contrato | comportamento na placa |
| Simulado na placa | firmware, tempos e comandos | sensores, motores e Bluetooth reais |
| Real com ToF mockado | fluxo completo, enlace e telas | navegação em labirinto real |
| Real com os três ToF, na pista | navegação, tempo e taxa de sucesso | só vale com bateria no padrão e memória apagada no começo |
