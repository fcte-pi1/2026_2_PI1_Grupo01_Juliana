#ifndef HAL_MOTOR_H
#define HAL_MOTOR_H

/* Motores DC N20 pela ponte H (PWM/LEDC + sentido) — FIRM-04 (#130) */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTOR_ESQ = 0,
    MOTOR_DIR,
    MOTOR_QTD
} motor_id_t;

#define MOTOR_POTENCIA_MAX 1000

/* Configura os pinos e deixa a ponte H em standby (motores desligados). */
void hal_motor_iniciar(void);

/* Liga/desliga a ponte H (STBY). Desligada, os motores não giram. */
void hal_motor_habilitar(bool habilitar);

/* Potência de -1000 (ré máxima) a +1000 (frente máxima); 0 para. */
void hal_motor_definir(motor_id_t motor, int16_t potencia);

#ifdef __cplusplus
}
#endif

#endif /* HAL_MOTOR_H */
