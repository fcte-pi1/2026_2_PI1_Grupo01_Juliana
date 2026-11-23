#if !defined(HAL_SIMULADA) && defined(ARDUINO)

#include <Arduino.h>

#include "hal_tempo.h"

uint32_t hal_tempo_ms(void) { return millis(); }

#endif
