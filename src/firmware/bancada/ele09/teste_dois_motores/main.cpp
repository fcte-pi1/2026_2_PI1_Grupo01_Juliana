/*
 * Teste dos DOIS motores com encoder - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Ponte H TB6612: PWMA 25, AIN1 26, AIN2 15 | PWMB 27, BIN1 14, BIN2 18 | STBY 13
 * Motor ESQUERDO: M1 -> AO1, M2 -> AO2, C1 -> D34, C2 -> D35
 * Motor DIREITO : M1 -> BO1, M2 -> BO2, C1 -> VN (GPIO39), C2 -> D32
 * Os dois encoders: VCC -> 3V3 da ESP32 (NUNCA na bateria), GND -> GND comum
 *
 * Sequencia (repete): esquerdo frente/re, direito frente/re, os dois frente/re.
 * A cada 0,5 s mostra a contagem e os pulsos/s de cada encoder.
 * Frente = contagem SUBINDO. Se um motor contar ao contrario, troque C1 com C2
 * dele (ou M1 com M2 se o motor girar para o lado errado).
 *
 * Alimentacao: LiPo 2S. VEL ate 180 (limite seguro com a bateria cheia).
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

#define VEL       150
#define TEMPO_MS  3000

volatile long cont_e = 0, cont_d = 0;
void IRAM_ATTR isr_e() { if (digitalRead(ENC_E_B)) cont_e--; else cont_e++; }
void IRAM_ATTR isr_d() { if (digitalRead(ENC_D_B)) cont_d--; else cont_d++; }

void motor(int pwm, int in1, int in2, int vel) {
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(vel));
}

void mover(const char *nome, int ve, int vd) {
  Serial.printf("\n== %s  (E=%d  D=%d) ==\n", nome, ve, vd);
  long e0 = cont_e, d0 = cont_d;
  motor(PWMA, AIN1, AIN2, ve);
  motor(PWMB, BIN1, BIN2, vd);
  uint32_t t0 = millis(), tu = t0;
  long eu = cont_e, du = cont_d;
  while (millis() - t0 < TEMPO_MS) {
    if (millis() - tu >= 500) {
      long e = cont_e, d = cont_d;
      Serial.printf("ESQ: cont=%7ld  pulsos/s=%6ld   |   DIR: cont=%7ld  pulsos/s=%6ld\n",
                    e, (e - eu) * 2, d, (d - du) * 2);
      eu = e; du = d; tu = millis();
    }
  }
  motor(PWMA, AIN1, AIN2, 0);
  motor(PWMB, BIN1, BIN2, 0);
  Serial.printf("Total: ESQ %+ld pulsos | DIR %+ld pulsos\n", cont_e - e0, cont_d - d0);
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  int saidas[] = {PWMA, AIN1, AIN2, PWMB, BIN1, BIN2, STBY};
  for (int p : saidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  int entradas[] = {ENC_E_A, ENC_E_B, ENC_D_A, ENC_D_B};
  for (int p : entradas) pinMode(p, INPUT);
  attachInterrupt(ENC_E_A, isr_e, RISING);
  attachInterrupt(ENC_D_A, isr_d, RISING);
  delay(1500);
  Serial.println("\nTeste dos dois motores com encoder");
  Serial.printf("Leitura inicial: ESQ C1=%d C2=%d | DIR C1=%d C2=%d\n",
                digitalRead(ENC_E_A), digitalRead(ENC_E_B), digitalRead(ENC_D_A), digitalRead(ENC_D_B));
  digitalWrite(STBY, HIGH);
}

void loop() {
  mover("Esquerdo frente", VEL, 0);
  mover("Esquerdo re", -VEL, 0);
  mover("Direito frente", 0, VEL);
  mover("Direito re", 0, -VEL);
  mover("Os dois frente", VEL, VEL);
  mover("Os dois re", -VEL, -VEL);
  Serial.println("\n---- fim do ciclo, recomecando em 3 s ----");
  delay(3000);
}
