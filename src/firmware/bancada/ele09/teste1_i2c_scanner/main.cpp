/*
 * Teste 1 - I2C Scanner (Dupla 3: Marcus e Fabio)
 * Ratatouille - PI1 2026.2
 *
 * Ligacao:  VL53L0X VIN -> 3V3 (NUNCA 5V)   GND -> GND
 *           SDA -> D21    SCL -> D22         XSHUT e GPIO1 soltos
 *
 * Monitor: pio device monitor (115200 baud). Esperado: "I2C device found at address 0x29"
 */
#include <Arduino.h>
#include <Wire.h>

#define PINO_SDA 21
#define PINO_SCL 22

void setup() {
  Serial.begin(115200);
  delay(1000);
  Wire.begin(PINO_SDA, PINO_SCL);
  Serial.println();
  Serial.println("I2C Scanner - Ratatouille (SDA=D21, SCL=D22)");
}

/*
 * Diagnostico sem multimetro: o modulo VL53L0X tem resistores de pull-up no
 * SDA e no SCL. Com o pull-down interno da ESP32 ligado, a linha so le HIGH se
 * esse pull-up estiver chegando, ou seja, se o modulo esta alimentado e aquele
 * fio esta ligado. LOW = fio solto, sensor sem energia ou pino sem contato.
 */
void diagnosticar_linhas() {
  Wire.end();
  pinMode(PINO_SDA, INPUT_PULLDOWN);
  pinMode(PINO_SCL, INPUT_PULLDOWN);
  delay(5);
  int sda = digitalRead(PINO_SDA);
  int scl = digitalRead(PINO_SCL);
  Serial.printf("  SDA (D21): %s\n", sda ? "HIGH -> fio e modulo OK" : "LOW  -> fio solto ou modulo sem energia");
  Serial.printf("  SCL (D22): %s\n", scl ? "HIGH -> fio e modulo OK" : "LOW  -> fio solto ou modulo sem energia");
  if (!sda && !scl) {
    Serial.println("  As duas LOW: confira primeiro o 3V3 e o GND do sensor");
  } else if (sda && scl) {
    Serial.println("  As duas HIGH: ligacao OK; confira SDA/SCL trocados ou o contato dos pinos sem solda");
  }
  Wire.begin(PINO_SDA, PINO_SCL);
}

/*
 * Le registros de identificacao do sensor no endereco 0x29. O scanner so testa
 * se o sensor responde ao endereco; aqui a ESP32 tambem LE dados dele, que e o
 * que a biblioteca faz. VL53L0X: 0xC0 = 0xEE, 0xC1 = 0xAA, 0xC2 = 0x10.
 */
void ler_identificacao() {
  const uint8_t regs[] = {0xC0, 0xC1, 0xC2};
  Serial.print("  ID do 0x29:");
  for (uint8_t r : regs) {
    Wire.beginTransmission(0x29);
    Wire.write(r);
    uint8_t e = Wire.endTransmission(false);   /* repeated start, como a biblioteca */
    uint8_t n = Wire.requestFrom((uint8_t)0x29, (uint8_t)1);
    if (e == 0 && n == 1) {
      Serial.printf("  0x%02X=0x%02X", r, Wire.read());
    } else {
      Serial.printf("  0x%02X=falha(e%u,n%u)", r, e, n);
    }
  }
  Serial.println("   (esperado VL53L0X: 0xEE 0xAA 0x10)");
}

void loop() {
  int encontrados = 0;
  Serial.println("Varrendo o barramento I2C...");

  for (uint8_t endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    uint8_t erro = Wire.endTransmission();
    if (erro == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", endereco);
      encontrados++;
      if (endereco == 0x29) ler_identificacao();
    }
  }

  if (encontrados == 0) {
    Serial.println("No I2C devices found (confira 3V3, GND, SDA e SCL)");
    diagnosticar_linhas();
  }
  Serial.println();
  delay(3000);  /* repete a cada 3 s */
}
