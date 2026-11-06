/*
 * Teste do motor ESQUERDO com encoder - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Ligacao do encoder do motor esquerdo (cabo de 6 fios do N20):
 *   M1  -> AO1 da ponte H        M2  -> AO2 da ponte H
 *   VCC -> 3V3 da ESP32 (NUNCA na bateria)
 *   GND -> GND comum
 *   C1  -> D34 (canal A)         C2  -> D35 (canal B)
 *
 * O que faz: gira o motor para frente e para tras e mostra, a cada 0,5 s,
 * os pulsos contados, o sentido lido pelo encoder e os pulsos por segundo.
 * Na frente a contagem deve SUBIR; na re deve DESCER. Se for ao contrario,
 * basta trocar C1 com C2 (ou inverter no firmware depois).
 *
 * D34 e D35 nao tem pull-up interno. A maioria dos encoders de N20 ja tem
 * resistor na plaquinha; se a contagem ficar sempre 0 ou maluca, falta
 * pull-up: um resistor de 10 k de cada canal (C1, C2) para o 3V3.
 *
 * Monitor: pio device monitor -b 115200
 */
#include <Arduino.h>

#define PWMA 25
#define AIN1 26
#define AIN2 15
#define STBY 13
#define ENC_A 34
#define ENC_B 35

#define VEL       150     /* 0 a 255 (maximo seguro na LiPo cheia: 180) */
#define TEMPO_MS  4000

volatile long contagem = 0;

/* Quadratura simples: na subida do canal A, o canal B diz o sentido. */
void IRAM_ATTR isr_a() {
  if (digitalRead(ENC_B)) contagem--; else contagem++;
}

void motor(int vel) {
  digitalWrite(AIN1, vel > 0 ? HIGH : LOW);
  digitalWrite(AIN2, vel < 0 ? HIGH : LOW);
  analogWrite(PWMA, abs(vel));
}

void rodar(int vel, const char *nome) {
  Serial.printf("\n== %s (PWM %d) ==\n", nome, vel);
  long antes = contagem;
  motor(vel);
  uint32_t t0 = millis(), ultimo = t0;
  long c_ultimo = contagem;
  while (millis() - t0 < TEMPO_MS) {
    if (millis() - ultimo >= 500) {
      long c = contagem;
      long pps = (c - c_ultimo) * 2;            /* pulsos em 0,5 s -> por segundo */
      Serial.printf("contagem=%7ld  pulsos/s=%6ld  sentido=%s\n", c, pps,
                    pps > 0 ? "FRENTE" : (pps < 0 ? "RE" : "parado"));
      c_ultimo = c; ultimo = millis();
    }
  }
  motor(0);
  Serial.printf("%s: total de pulsos = %ld\n", nome, contagem - antes);
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  int saidas[] = {PWMA, AIN1, AIN2, STBY};
  for (int p : saidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  attachInterrupt(ENC_A, isr_a, RISING);
  delay(1500);
  Serial.println("\nTeste do encoder do motor esquerdo");
  Serial.printf("Leitura inicial: C1(D34)=%d  C2(D35)=%d\n", digitalRead(ENC_A), digitalRead(ENC_B));
  digitalWrite(STBY, HIGH);
}

void loop() {
  rodar(VEL, "Frente");
  rodar(-VEL, "Re");
}
