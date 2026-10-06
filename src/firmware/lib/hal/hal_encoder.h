#ifndef HAL_ENCODER_H
#define HAL_ENCODER_H

/* Encoders A/B dos motores (contador PCNT) — FIRM-04 (#130) */

#include <stdint.h>
#include "hal_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

void hal_encoder_iniciar(void);

/* Total de pulsos desde o início; cresce para a frente e diminui na ré. */
int32_t hal_encoder_ler(motor_id_t motor);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ENCODER_H */
