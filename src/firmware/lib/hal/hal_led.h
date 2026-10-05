#ifndef HAL_LED_H
#define HAL_LED_H

/* LED de status e buzzer — FIRM-09 (#138) */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_led_iniciar(void);
void hal_led_definir(bool aceso);
void hal_buzzer_definir(bool ligado);

#ifdef __cplusplus
}
#endif

#endif /* HAL_LED_H */
