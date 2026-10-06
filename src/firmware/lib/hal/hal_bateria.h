#ifndef HAL_BATERIA_H
#define HAL_BATERIA_H

/* Leitura do divisor de tensão da bateria pelo ADC — FIRM-06 (#128) */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_bateria_iniciar(void);

/*
 * Tensão no pino do ADC, em mV (já com a calibração de fábrica).
 * A conversão para a tensão da bateria e para % fica fora da HAL.
 */
uint16_t hal_bateria_ler_adc_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BATERIA_H */
