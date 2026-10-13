#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Leitura da bateria na ESP32 (FIRM-06, #128): divisor 22 kΩ / 10 kΩ no GPIO36.
 *
 * analogReadMilliVolts() já aplica a calibração de fábrica gravada no eFuse
 * (esp_adc_cal no arduino-esp32 2.x). Com 11 dB o ADC1 lê até ~3,1 V; a LiPo
 * cheia dá 2,52 V no pino (tabela 10 do 4.2).
 */

#include <Arduino.h>

#include "config/pinos.h"
#include "hal_bateria.h"

void hal_bateria_iniciar(void)
{
    analogReadResolution(12);
    analogSetPinAttenuation(PINO_VBAT_SENSE, ADC_11db);
}

uint16_t hal_bateria_ler_adc_mv(void) { return (uint16_t)analogReadMilliVolts(PINO_VBAT_SENSE); }

#endif
