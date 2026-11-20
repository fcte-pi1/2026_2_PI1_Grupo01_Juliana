#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Botão BOOT da DevKit no GPIO0: pull-up interno, apertado = nível baixo. Depois do boot o
 * GPIO0 é uma entrada comum; apertar com a placa ligando é que entra no modo de gravação.
 */

#include <Arduino.h>

#include "config/pinos.h"
#include "hal_boot.h"

void hal_boot_iniciar(void) { pinMode(PINO_BOOT, INPUT_PULLUP); }

bool hal_boot_pressionado(void) { return digitalRead(PINO_BOOT) == LOW; }

#endif
