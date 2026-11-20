#ifndef MOTOR_AUTOTESTE_H
#define MOTOR_AUTOTESTE_H

/*
 * Autoteste de motor e encoder para o health-check.
 *
 * Gira uma roda por vez, MOTOR_AUTOTESTE_MS a MOTOR_AUTOTESTE_POTENCIA, e conta
 * os pulsos do encoder dela. O contrato usa essa contagem
 * como o `valor` do encoder e aprova o motor pelo próprio encoder: se a roda
 * girou e o encoder contou, os dois estão bons.
 *
 * Rodas no ar. A ponte H só fica habilitada durante o giro e volta ao standby
 * ao fim. Quem chama garante que o robô está parado (mov_parar).
 */

#include <stdbool.h>
#include <stdint.h>

#include "hal_motor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Espera ms milissegundos (vTaskDelay na ESP32; na simulação, avança o mundo). */
typedef void (*hc_esperar_t)(uint32_t ms);

/* Pulsos contados por cada roda no giro dela, em valor absoluto. */
void motor_autoteste_executar(hc_esperar_t esperar, int32_t pulsos[MOTOR_QTD]);

/* true se a contagem alcança MOTOR_AUTOTESTE_MIN_PULSOS. */
bool motor_autoteste_aprovado(int32_t pulsos);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_AUTOTESTE_H */
