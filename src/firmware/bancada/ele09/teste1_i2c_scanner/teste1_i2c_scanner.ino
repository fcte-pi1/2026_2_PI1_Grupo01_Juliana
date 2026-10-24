/*
 * Teste 1 - I2C Scanner (Dupla 3: Marcus e Fabio)
 * Ratatouille - PI1 2026.2
 *
 * Ligacao:  VL53L0X VIN -> 3V3 (NUNCA 5V)   GND -> GND
 *           SDA -> D21    SCL -> D22         XSHUT e GPIO1 soltos
 *
 * Monitor Serial: 115200 baud. Esperado: "I2C device found at address 0x29"
 */
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

void loop() {
  int encontrados = 0;
  Serial.println("Varrendo o barramento I2C...");

  for (uint8_t endereco = 1; endereco < 127; endereco++) {
    Wire.beginTransmission(endereco);
    uint8_t erro = Wire.endTransmission();
    if (erro == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", endereco);
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println("No I2C devices found (confira 3V3, GND, SDA e SCL)");
  }
  Serial.println();
  delay(3000);  /* repete a cada 3 s */
}
