#ifndef VELOCIDADE_H
#define VELOCIDADE_H

/*
 * Velocidade por roda — FIRM-04 (#130).
 *
 * vel_passo() roda a 200 Hz: lê o encoder, compara com a referência em mm/s
 * e manda a potência à ponte H (feedforward do regime do motor + PI).
 * Potência aplicada com o encoder parado por MOTOR_TRAVADO_MS desliga a
 * ponte até o próximo vel_iniciar(). Referência 0 não arma esse prazo.
 *
 * Quem chama vel_passo() no robô entra depois. Este módulo não dirige
 * o movimento de célula.
 */

#include <stdbool.h>
#include <stdint.h>

#include "hal_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicia motores e encoders, com a ponte H em standby, e zera o PI. */
void vel_iniciar(void);

/* Guarda a referência. O PWM só muda no próximo vel_passo(). */
void vel_definir_mm_s(motor_id_t motor, float mm_s);

/* Um passo de controle, quando já se passaram 5 ms. */
void vel_passo(void);

/* true depois do travamento, até vel_iniciar(). */
bool vel_travado(void);

/* Última velocidade medida no encoder, em mm/s. */
float vel_mm_s(motor_id_t motor);

#ifdef __cplusplus
}
#endif

#endif /* VELOCIDADE_H */
