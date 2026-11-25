/*
 * TESTE GERAL das placas A + B - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Testa de uma vez: 3 sensores VL53L0X, chave DIP, buzzer,
 * tensao da bateria, motores pela ponte H e encoders.
 *
 * SEGURANCA: os motores SO giram com a CHAVE 1 da DIP em ON.
 * Deixe o robo levantado (rodas no ar). Chave 1 em OFF = motores parados.
 *
 * Pinos (placas A e B):
 *   I2C: SDA 21, SCL 22 | XSHUT: esq D4, frente D33, dir D5
 *   DIP: 1=D23  2=GPIO17(TX2)  3=GPIO16(RX2)  4=D19  (ON = ligado ao GND)
 *   Buzzer: GPIO12 (via transistor) | Bateria: GPIO36 (divisor 22k/10k)
 *   Ponte H: PWMA 25, AIN1 26, AIN2 15 | PWMB 27, BIN1 14, BIN2 18 | STBY 13
 *   Encoders: ESQ A=34 B=35 | DIR A=39(VN) B=32
 *
 * Monitor: pio device monitor -b 115200
 */
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>

// ---------- pinos ----------
#define SDA_PIN 21
#define SCL_PIN 22
const int XSHUT[3] = {4, 33, 5};               // esquerdo, frente, direito
const char *NOME_SENSOR[3] = {"ESQ", "FRENTE", "DIR"};
const uint8_t ENDERECO[3] = {0x30, 0x31, 0x32};

const int DIP[4] = {23, 17, 16, 19};

#define BUZZER 12
#define VBAT_PIN 36
#define VBAT_FATOR 3.2f                        // (22k + 10k) / 10k

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

#define LED 2
#define VEL 150                                // PWM (max seguro 180 com 2S)

// ---------- encoders ----------
volatile long cont_e = 0, cont_d = 0;
void IRAM_ATTR isr_e() { if (digitalRead(ENC_E_B)) cont_e--; else cont_e++; }
void IRAM_ATTR isr_d() { if (digitalRead(ENC_D_B)) cont_d--; else cont_d++; }

// ---------- sensores ----------
VL53L0X sensor[3];
bool sensor_ok[3] = {false, false, false};

void iniciar_sensores() {
  for (int i = 0; i < 3; i++) { pinMode(XSHUT[i], OUTPUT); digitalWrite(XSHUT[i], LOW); }
  delay(50);
  for (int i = 0; i < 3; i++) {
    digitalWrite(XSHUT[i], HIGH);              // liga um sensor por vez
    delay(20);
    sensor[i].setTimeout(100);
    bool ok = false;
    for (int t = 0; t < 3 && !ok; t++) { ok = sensor[i].init(); if (!ok) delay(30); }
    if (ok) {
      sensor[i].setAddress(ENDERECO[i]);       // cada um com endereco proprio
      sensor[i].startContinuous(30);
      sensor_ok[i] = true;
      Serial.printf("Sensor %-6s OK (0x%02X)\n", NOME_SENSOR[i], ENDERECO[i]);
    } else {
      Serial.printf("Sensor %-6s FALHOU - confira VIN/GND/SDA/SCL e o XSHUT (GPIO %d)\n",
                    NOME_SENSOR[i], XSHUT[i]);
    }
  }
}

// ---------- motores ----------
void motor(int pwm, int in1, int in2, int vel) {
  vel = constrain(vel, -180, 180);
  digitalWrite(in1, vel > 0 ? HIGH : LOW);
  digitalWrite(in2, vel < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(vel));
}
void motores(int v) { motor(PWMA, AIN1, AIN2, v); motor(PWMB, BIN1, BIN2, v); }

// ---------- buzzer ----------
void bip(int freq, int ms) { tone(BUZZER, freq, ms); delay(ms + 30); }

// ---------- bateria ----------
float ler_bateria() {
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(VBAT_PIN);
  return (mv / 8.0f) * VBAT_FATOR / 1000.0f;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n===== TESTE GERAL - Ratatouille =====");

  pinMode(LED, OUTPUT);
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  motores(0);
  digitalWrite(STBY, HIGH);

  for (int i = 0; i < 4; i++) pinMode(DIP[i], INPUT_PULLUP);

  pinMode(ENC_E_A, INPUT); pinMode(ENC_E_B, INPUT);   // 34/35/39 nao tem pull-up interno
  pinMode(ENC_D_A, INPUT); pinMode(ENC_D_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_E_A), isr_e, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_D_A), isr_d, RISING);

  analogSetPinAttenuation(VBAT_PIN, ADC_11db);

  bip(2000, 120); bip(2600, 120);              // se ouvir 2 bips, o buzzer funciona

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(50000);
  iniciar_sensores();

  Serial.printf("Bateria: %.2f V\n", ler_bateria());
  Serial.println("Chave 1 em ON = motores giram (frente 2 s, para, re 2 s, para).");
  Serial.println("--------------------------------------");
}

// maquina de estados dos motores (sem travar o loop)
int fase = 0;
uint32_t t_fase = 0;
const int VEL_FASE[4] = {VEL, 0, -VEL, 0};
const uint32_t DUR_FASE[4] = {2000, 1000, 2000, 1000};
const char *NOME_FASE[4] = {"FRENTE", "parado", "RE", "parado"};

void loop() {
  bool dip[4];
  for (int i = 0; i < 4; i++) dip[i] = (digitalRead(DIP[i]) == LOW);  // ON = GND

  // motores
  if (dip[0]) {
    if (millis() - t_fase > DUR_FASE[fase]) { fase = (fase + 1) % 4; t_fase = millis(); }
    motores(VEL_FASE[fase]);
  } else {
    motores(0); fase = 0; t_fase = millis();
  }

  static uint32_t t_print = 0;
  if (millis() - t_print >= 300) {
    t_print = millis();
    digitalWrite(LED, !digitalRead(LED));

    Serial.print("Dist(mm) ");
    for (int i = 0; i < 3; i++) {
      if (!sensor_ok[i]) { Serial.printf("%s:----  ", NOME_SENSOR[i]); continue; }
      uint16_t d = sensor[i].readRangeContinuousMillimeters();
      if (sensor[i].timeoutOccurred() || d >= 8000) Serial.printf("%s:fora  ", NOME_SENSOR[i]);
      else Serial.printf("%s:%4u  ", NOME_SENSOR[i], d);
    }
    Serial.printf("| DIP %d%d%d%d ", dip[0], dip[1], dip[2], dip[3]);
    Serial.printf("| Bat %.2fV ", ler_bateria());
    Serial.printf("| Enc E:%6ld D:%6ld ", cont_e, cont_d);
    Serial.printf("| Mot %s\n", dip[0] ? NOME_FASE[fase] : "OFF");
  }
}
