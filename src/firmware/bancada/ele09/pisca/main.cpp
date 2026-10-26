/*
 * Pisca - Ratatouille PI1 2026.2 (programa a parte)
 * Teste de alimentacao: com a ESP32 ligada pela fonte (VIN), sem USB,
 * o LED azul da placa (D2) pisca 1 vez por segundo se ela estiver funcionando.
 */
#include <Arduino.h>

#define PINO_LED 2   /* LED azul da propria placa */

void setup() {
  pinMode(PINO_LED, OUTPUT);
}

void loop() {
  digitalWrite(PINO_LED, HIGH);
  delay(500);
  digitalWrite(PINO_LED, LOW);
  delay(500);
}
