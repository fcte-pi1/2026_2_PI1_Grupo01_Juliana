/*
 * Teste da ponte H TB6612 (HW-166) com os 2 motores N20 - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Pinos (os mesmos do layout das placas / include/config/pinos.h):
 *   Motor esquerdo (canal A): PWMA = D25, AIN1 = D26, AIN2 = D15
 *   Motor direito  (canal B): PWMB = D27, BIN1 = D14, BIN2 = D18
 *   STBY = D13 (nivel alto liga a ponte)
 *   Encoders (opcional): E = D34/D35, D = VN(39)/D32  -> VCC do encoder no 3V3
 *
 * Sequencia (repete): cada motor sozinho para frente e para tras, depois os
 * dois juntos, sempre com pausa parado entre os movimentos.
 * Monitor: pio device monitor -b 115200
 */
#include <Arduino.h>

#define PWMA 25
#define AIN1 26
#define AIN2 15
#define PWMB 27
#define BIN1 14
#define BIN2 18
#define STBY 13

#define ENC_E_A 34
#define ENC_E_B 35
#define ENC_D_A 39
#define ENC_D_B 32

#define VELOCIDADE 150      /* 0 a 255; comece baixo */
#define TEMPO_MS   2000

volatile long pulsos_e = 0, pulsos_d = 0;
void IRAM_ATTR isr_e() { pulsos_e++; }
void IRAM_ATTR isr_d() { pulsos_d++; }

/* vel > 0 frente, < 0 re, 0 parado */
void motor(int pwm, int in1, int in2, int vel) {
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(vel));
}

void mover(const char *nome, int ve, int vd) {
  pulsos_e = 0; pulsos_d = 0;
  Serial.printf("%-22s E=%4d D=%4d ... ", nome, ve, vd);
  motor(PWMA, AIN1, AIN2, ve);
  motor(PWMB, BIN1, BIN2, vd);
  delay(TEMPO_MS);
  motor(PWMA, AIN1, AIN2, 0);
  motor(PWMB, BIN1, BIN2, 0);
  Serial.printf("pulsos encoder E=%ld D=%ld\n", pulsos_e, pulsos_d);
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  int saidas[] = {PWMA, AIN1, AIN2, PWMB, BIN1, BIN2, STBY};
  for (int p : saidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  pinMode(ENC_E_A, INPUT); pinMode(ENC_E_B, INPUT);
  pinMode(ENC_D_A, INPUT); pinMode(ENC_D_B, INPUT);
  attachInterrupt(ENC_E_A, isr_e, RISING);
  attachInterrupt(ENC_D_A, isr_d, RISING);
  delay(1500);
  Serial.println("\nTeste da ponte H - ligando STBY");
  digitalWrite(STBY, HIGH);
}

void loop() {
  mover("Esquerdo frente", VELOCIDADE, 0);
  mover("Esquerdo re", -VELOCIDADE, 0);
  mover("Direito frente", 0, VELOCIDADE);
  mover("Direito re", 0, -VELOCIDADE);
  mover("Os dois frente", VELOCIDADE, VELOCIDADE);
  mover("Os dois re", -VELOCIDADE, -VELOCIDADE);
  Serial.println("---- fim do ciclo, recomecando em 3 s ----");
  delay(3000);
}
