/*
 * Teste 2 - Filtro de media movel + limiar com LED (Dupla 3: Marcus e Fabio)
 * Ratatouille - PI1 2026.2
 *
 * Biblioteca: Adafruit VL53L0X (o PlatformIO baixa sozinho pelo lib_deps).
 *
 * Ligacao:  VL53L0X VIN -> 3V3, GND -> GND, SDA -> D21, SCL -> D22
 *           D2 -> resistor 1k -> perna LONGA do LED; perna CURTA -> GND
 *
 * Grafico: extensao Teleplot do VS Code (115200). Cada linha ">nome:valor"
 * vira uma curva: bruto, filtrado e o limiar (100 mm). Feche o monitor serial
 * do PlatformIO antes de conectar a Teleplot: so um programa usa a porta.
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define PINO_SDA      21
#define PINO_SCL      22
#define PINO_LED      2
#define LIMIAR_MM     100   /* 10 cm */
#define N_MEDIA       5     /* janela da media movel */
#define ALCANCE_MAX   2000  /* leitura "fora de alcance" vira 2000 mm */

Adafruit_VL53L0X sensor;

uint16_t janela[N_MEDIA];   /* ultimas 5 leituras */
uint8_t  posicao = 0;       /* onde entra a proxima leitura */
uint8_t  quantidade = 0;    /* quantas posicoes ja foram preenchidas */
uint32_t soma = 0;          /* soma das leituras da janela */

/* Coloca uma leitura nova na janela e devolve a media das ultimas N_MEDIA. */
uint16_t media_movel(uint16_t leitura) {
  if (quantidade == N_MEDIA) {
    soma -= janela[posicao];        /* tira a leitura mais antiga */
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

  Wire.begin(PINO_SDA, PINO_SCL);
  /* 50 kHz em vez de 100 kHz: mais tolerante aos jumpers longos, emendas e
   * pinos sem solda da bancada */
  Wire.setClock(50000);
  delay(100);   /* da tempo do sensor terminar de ligar */
  /* Tenta de novo a cada 1 s em vez de travar: com os pinos do sensor sem
   * solda, o contato pode falhar na primeira tentativa. */
  while (!sensor.begin()) {
    Serial.println("VL53L0X nao encontrado, tentando de novo (confira o contato do SDA/SCL)");
    delay(1000);
  }
  Serial.println("VL53L0X encontrado");
}

void loop() {
  VL53L0X_RangingMeasurementData_t medida = {};   /* zera: sem lixo de leituras antigas */
  VL53L0X_Error erro = sensor.rangingTest(&medida, false);

  /* Leitura que falhou (contato do I2C) e descartada: sem isso o codigo
   * repetia o ultimo valor e o grafico parecia travado. */
  if (erro != VL53L0X_ERROR_NONE) {
    return;
  }

  /* RangeStatus 4 = fora de alcance (nada na frente): conta como longe. */
  uint16_t bruto = (medida.RangeStatus == 4) ? ALCANCE_MAX : medida.RangeMilliMeter;
  if (bruto > ALCANCE_MAX) bruto = ALCANCE_MAX;

  uint16_t filtrado = media_movel(bruto);

  /* Limiar: parede a menos de 10 cm acende o LED. */
  if (filtrado < LIMIAR_MM) {
    digitalWrite(PINO_LED, HIGH);
  } else {
    digitalWrite(PINO_LED, LOW);
  }

  /* Formato da Teleplot: ">nome:valor", uma curva por nome. */
  Serial.printf(">bruto:%u\n>filtrado:%u\n>limiar:%u\n", bruto, filtrado, LIMIAR_MM);
}
