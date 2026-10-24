# ELE-09: testes de bancada dos sensores ToF e da telemetria Bluetooth

Programas **à parte** da Eletrônica (Marcus e Fábio), seguindo o roteiro de testes da Dupla 3 (Sensores e Comunicações). Não são o firmware do robô: não usam a HAL nem entram no `platformio.ini`. São sketches para a **Arduino IDE**, que tem o Plotter Serial pedido no Teste 2.

Os pinos são os mesmos de `include/config/pinos.h`: `SDA = 21`, `SCL = 22`, `LED_STATUS = 2`.

## Preparação

1. Arduino IDE → **Gerenciador de Placas** → instalar **esp32** (Espressif).
2. **Ferramentas → Placa → esp32 → DOIT ESP32 DEVKIT V1** e escolher a porta da placa.
3. **Gerenciador de Bibliotecas** → instalar **Adafruit VL53L0X** (aceitar as dependências, como a Adafruit BusIO). Só os Testes 2 e 3 usam.

Cada teste fica numa pasta com o mesmo nome do `.ino`, como a Arduino IDE exige: basta abrir o `.ino`.

## Ligação

| VL53L0X | ESP32 |
|---|---|
| VIN | **3V3** (nunca 5 V: os pull-ups do I²C do módulo levariam 5 V aos pinos da ESP32) |
| GND | GND |
| SDA | D21 |
| SCL | D22 |
| XSHUT, GPIO1 | soltos |

Testes 2 e 3: **D2 → resistor de 1 kΩ → perna longa do LED; perna curta → GND**.

## Testes

| Teste | Pasta | O que faz | Registro obrigatório |
|---|---|---|---|
| 1 | `teste1_i2c_scanner` | Varre o barramento a cada 3 s | Print do Monitor Serial (115200) com `I2C device found at address 0x29` |
| 2 | `teste2_filtro_limiar` | Média móvel das últimas 5 leituras; LED aceso abaixo de 100 mm | Vídeo do LED acendendo/apagando na marca de 10 cm da régua + print do Plotter Serial |
| 3 | `teste3_bluetooth` | Envia a distância filtrada pelo Bluetooth (`Ratatuli_ToF`) a 10 Hz | Gravação de tela do celular recebendo os dados |

### Detalhes

- **Fora de alcance:** quando nada está na frente, o VL53L0X devolve `RangeStatus = 4`. O código conta essa leitura como 2000 mm, para a média subir e o LED apagar. Se a leitura fosse descartada, a média ficaria presa no último valor perto.
- **Plotter Serial (Teste 2):** o código imprime `bruto:…,filtrado:…,limiar:100`, e a Arduino IDE desenha as três curvas. O print mostra a curva filtrada suave sobre a bruta e a linha do limiar.
- **10 Hz (Teste 3):** um `delay(100)` puro somaria o tempo de leitura do sensor (cerca de 30 ms) e daria cerca de 7,5 Hz. O laço usa `millis()` para fechar 100 ms por ciclo. No app *Serial Bluetooth Terminal*, ligar o horário das mensagens (*Settings → Terminal → Timestamp*) mostra o intervalo de 0,1 s na gravação.
- **Celular:** o Bluetooth do ESP32 aqui é o clássico (SPP). Ele funciona com Android; **o iPhone não conecta**.

## Resultados

| Teste | Data | Resultado | Evidência |
|---|---|---|---|
| 1 | | | |
| 2 | | | |
| 3 | | | |
