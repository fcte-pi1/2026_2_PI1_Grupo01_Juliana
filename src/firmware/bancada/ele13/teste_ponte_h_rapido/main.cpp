/*
 * Teste da ponte H TB6612 (HW-166) - versao mais rapida - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Alimentacao: LiPo 2S (7,4 V nominal, 8,4 V cheia). Os N20 sao de 6 V.
 * Pelo projeto de energia (4.2), o PWM maximo e 6,0 V / V_bateria:
 *   8,4 V (cheia) -> 71 % = 181 de 255   |   7,4 V -> 81 % = 207 de 255
 * Por isso a velocidade maxima aqui e 180: segura mesmo com a bateria cheia.
 * NAO aumente VEL_MAX sem medir a bateria.
 *
 * Pinos: PWMA 25, AIN1 26, AIN2 15 | PWMB 27, BIN1 14, BIN2 18 | STBY 13
 *
 * Sequencia: acelera devagar ate VEL_MAX (rampa, poupa motor e bateria),
 * mantem 3 s, desacelera, para; depois o mesmo para tras.
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

/* 1 = so esquerdo, 2 = so direito, 3 = os dois */
#define MOTORES   3

#define VEL_MAX   180     /* limite seguro com a LiPo cheia (ver acima) */
#define PASSO     5       /* quanto o PWM sobe a cada passo da rampa */
#define PASSO_MS  30      /* tempo de cada passo: 0 a 180 em ~1 s */
#define TEMPO_MS  3000    /* tempo na velocidade maxima */

void motor(int pwm, int in1, int in2, int vel) {
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(vel));
}

void aplicar(int vel) {
  int ve = (MOTORES == 1 || MOTORES == 3) ? vel : 0;
  int vd = (MOTORES == 2 || MOTORES == 3) ? vel : 0;
  motor(PWMA, AIN1, AIN2, ve);
  motor(PWMB, BIN1, BIN2, vd);
}

/* sentido = +1 frente, -1 re */
void correr(int sentido, const char *nome) {
  Serial.printf("%s: acelerando ate %d...\n", nome, VEL_MAX);
  for (int v = 0; v <= VEL_MAX; v += PASSO) { aplicar(sentido * v); delay(PASSO_MS); }
  aplicar(sentido * VEL_MAX);
  Serial.printf("%s: velocidade maxima (%d de 255)\n", nome, VEL_MAX);
  delay(TEMPO_MS);
  for (int v = VEL_MAX; v >= 0; v -= PASSO) { aplicar(sentido * v); delay(PASSO_MS); }
  aplicar(0);
  Serial.printf("%s: parado\n", nome);
  delay(1500);
}

void setup() {
  Serial.begin(115200);
  int saidas[] = {PWMA, AIN1, AIN2, PWMB, BIN1, BIN2, STBY};
  for (int p : saidas) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
  delay(1500);
  Serial.println("\nTeste rapido da ponte H - ligando STBY");
  digitalWrite(STBY, HIGH);
}

void loop() {
  correr(+1, "Frente");
  correr(-1, "Re");
  Serial.println("---- recomecando ----");
  delay(2000);
}
