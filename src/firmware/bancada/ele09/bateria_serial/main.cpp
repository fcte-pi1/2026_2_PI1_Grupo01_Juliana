/*
 * Teste de bateria (Monitor Serial) - Ratatouille PI1 2026.2 (programa a parte)
 *
 * Le a tensao da bateria no GPIO36 (PINO_VBAT_SENSE) uma vez por segundo e
 * mostra tensao, carga estimada (curva LiPo 2S do robo.h) e o tempo desde o
 * inicio. Com o motor girando, da para ver a tensao caindo com o consumo.

 *
 * LIGACAO DO DIVISOR (OBRIGATORIO, nunca a bateria direto no GPIO36):
 *   + bateria -> resistor 22k -> GPIO36 (VP) -> resistor 10k -> GND
 *   GND da bateria ligado ao GND da ESP32.
 *   Com 8,4 V (LiPo cheia) o GPIO36 recebe 8,4 x 10/32 = 2,6 V: seguro.
 *
 * MOTOR (opcional): RODAR_MOTOR 1 gira o motor com PWM fixo para gerar consumo,
 * usando os pinos da bancada do teste_motor_rampa (TB6612 canal A).
 */
#include <Arduino.h>


#define PINO_VBAT        36      /* VP, ADC1_CH0 */
#define DIVISOR          3.2f    /* 22k / 10k: V_bateria = V_pino x 3,2 */
#define AMOSTRAS         16      /* media contra ruido */
#define PERIODO_MS       1000    /* uma leitura por segundo */

#define RODAR_MOTOR      1       /* 0 = so mede; 1 = motor girando para ter consumo */
#define VELOCIDADE       200     /* PWM de 0 a 255 */
#define AIN1             18
#define AIN2             19
#define PWMA             21
#define STBY             22



/* Curva LiPo 2S do robo.h: tensao (mV) -> carga (%) */
const uint16_t curva[][2] = {
  {8400, 100}, {8200, 90}, {8000, 75}, {7800, 55},
  {7600, 35},  {7400, 15}, {7200, 5},  {7000, 0}
};
const int N_CURVA = sizeof(curva) / sizeof(curva[0]);

uint32_t ler_bateria_mv() {
  uint32_t soma = 0;
  for (int i = 0; i < AMOSTRAS; i++) {
    soma += analogReadMilliVolts(PINO_VBAT);   /* ja usa a calibracao do ADC */
    delay(2);
  }
  return (uint32_t)((soma / AMOSTRAS) * DIVISOR);
}

int carga_percentual(uint32_t mv) {
  if (mv >= curva[0][0]) return 100;
  if (mv <= curva[N_CURVA - 1][0]) return 0;
  for (int i = 0; i < N_CURVA - 1; i++) {
    if (mv >= curva[i + 1][0]) {   /* interpola entre dois pontos */
      int32_t v0 = curva[i + 1][0], v1 = curva[i][0];
      int32_t p0 = curva[i + 1][1], p1 = curva[i][1];
      return p0 + (int32_t)(mv - v0) * (p1 - p0) / (v1 - v0);
    }
  }
  return 0;
}

void setup() {
  Serial.begin(115200);

  analogSetPinAttenuation(PINO_VBAT, ADC_11db);   /* faixa ate ~3,1 V */

#if RODAR_MOTOR
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, VELOCIDADE);
#endif

  Serial.println("tempo_s,bateria_mV,carga_%");

}

void loop() {
  static uint32_t proxima_ms = millis();
  uint32_t mv = ler_bateria_mv();
  int carga = carga_percentual(mv);
  uint32_t t = millis() / 1000;

  Serial.printf("%lu,%lu,%d\n", (unsigned long)t, (unsigned long)mv, carga);


  proxima_ms += PERIODO_MS;
  int32_t falta = (int32_t)(proxima_ms - millis());
  if (falta > 0) delay(falta); else proxima_ms = millis();
}
