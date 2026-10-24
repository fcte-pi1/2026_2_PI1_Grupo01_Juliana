/*
 * Teste 3 - Telemetria por Bluetooth a 10 Hz (Dupla 3: Marcus e Fabio)
 * Ratatouille - PI1 2026.2
 *
 * Mesmo filtro e LED do Teste 2, mas a distancia filtrada vai pelo Bluetooth
 * (dispositivo "Ratatuli_ToF"). App: "Serial Bluetooth Terminal" (Android).
 * Atencao: o iPhone NAO conecta no Bluetooth classico (SPP) do ESP32.
 *
 * Ligacao: igual ao Teste 2.
 */
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth desativado: escolha a placa 'DOIT ESP32 DEVKIT V1' (ESP32 classico)"
#endif

#define PINO_SDA      21
#define PINO_SCL      22
#define PINO_LED      2
#define LIMIAR_MM     100
#define N_MEDIA       5
#define ALCANCE_MAX   2000
#define PERIODO_MS    100   /* 100 ms = 10 Hz */

Adafruit_VL53L0X sensor;
BluetoothSerial SerialBT;

uint16_t janela[N_MEDIA];
uint8_t  posicao = 0;
uint8_t  quantidade = 0;
uint32_t soma = 0;
uint32_t proxima_ms = 0;

uint16_t media_movel(uint16_t leitura) {
  if (quantidade == N_MEDIA) {
    soma -= janela[posicao];
  } else {
    quantidade++;
  }
  janela[posicao] = leitura;
  soma += leitura;
  posicao = (posicao + 1) % N_MEDIA;
  return soma / quantidade;
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  digitalWrite(PINO_LED, LOW);

  SerialBT.begin("Ratatuli_ToF");   /* nome que aparece no celular */

  Wire.begin(PINO_SDA, PINO_SCL);
  if (!sensor.begin()) {
    Serial.println("VL53L0X nao encontrado: confira a ligacao e rode o Teste 1");
    SerialBT.println("VL53L0X nao encontrado");
    while (true) delay(100);
  }
  Serial.println("Bluetooth pronto: pareie com 'Ratatuli_ToF'");
  proxima_ms = millis();
}

void loop() {
  VL53L0X_RangingMeasurementData_t medida;
  sensor.rangingTest(&medida, false);

  uint16_t bruto = (medida.RangeStatus == 4) ? ALCANCE_MAX : medida.RangeMilliMeter;
  if (bruto > ALCANCE_MAX) bruto = ALCANCE_MAX;
  uint16_t filtrado = media_movel(bruto);

  digitalWrite(PINO_LED, filtrado < LIMIAR_MM ? HIGH : LOW);

  SerialBT.println(filtrado);   /* telemetria pelo radio, nao pelo cabo */

  /* Taxa fixa de 10 Hz. Um delay(100) puro somaria o tempo da leitura do
   * sensor (~30 ms) e daria ~7,5 Hz; aqui o laco espera so o que falta
   * para completar 100 ms desde o inicio do ciclo anterior. */
  proxima_ms += PERIODO_MS;
  int32_t falta = (int32_t)(proxima_ms - millis());
  if (falta > 0) {
    delay(falta);
  } else {
    proxima_ms = millis();      /* atrasou: recomeca a contagem */
  }
}
