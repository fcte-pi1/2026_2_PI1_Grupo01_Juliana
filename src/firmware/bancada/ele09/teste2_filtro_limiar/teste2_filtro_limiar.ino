/*
 * Teste 2 - Filtro de media movel + limiar com LED (Dupla 3: Marcus e Fabio)
 * Ratatouille - PI1 2026.2
 *
 * Biblioteca: "Adafruit VL53L0X" (Gerenciador de Bibliotecas; aceite instalar
 *             as dependencias, como a Adafruit BusIO).
 *
 * Ligacao:  VL53L0X VIN -> 3V3, GND -> GND, SDA -> D21, SCL -> D22
 *           D2 -> resistor 1k -> perna LONGA do LED; perna CURTA -> GND
 *
 * Plotter Serial (115200): mostra 3 curvas - bruto, filtrado e o limiar (100 mm).
 */
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
  if (!sensor.begin()) {
    Serial.println("VL53L0X nao encontrado: confira a ligacao e rode o Teste 1");
    while (true) delay(100);
  }
}

void loop() {
  VL53L0X_RangingMeasurementData_t medida;
  sensor.rangingTest(&medida, false);

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

  /* Formato "nome:valor" -> o Plotter Serial desenha uma curva para cada um. */
  Serial.printf("bruto:%u,filtrado:%u,limiar:%u\n", bruto, filtrado, LIMIAR_MM);
}
