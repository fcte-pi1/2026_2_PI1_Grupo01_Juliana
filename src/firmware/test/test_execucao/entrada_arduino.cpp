#ifdef ARDUINO

/* Entrada dos testes Unity na ESP32: pio test -e esp32_sim -f test_execucao */

#include <Arduino.h>

extern "C" int executar_testes(void);

void setup(void)
{
    delay(2000); /* dá tempo de o monitor serial abrir */
    executar_testes();
}

void loop(void) {}

#endif
