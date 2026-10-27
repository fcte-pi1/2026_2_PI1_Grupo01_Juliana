#include <Preferences.h>

Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(1000);

  preferences.begin("teste", false);

  if (!preferences.isKey("sensor")) {
    preferences.putInt("sensor", 42);
    Serial.println("Valor 42 gravado pela primeira vez");
  }

  int valor = preferences.getInt("sensor", 0);

  Serial.print("Valor recuperado: ");
  Serial.println(valor);

  preferences.end();
}

void loop() {
}
