#ifndef APP_EVENTOS_H
#define APP_EVENTOS_H

/*
 * Mensagens da tarefa de navegação para a de telemetria, pela fila do FreeRTOS.
 * A navegação só coloca o evento na fila e segue em frente: ela nunca espera
 * o Bluetooth (RNF07).
 */

#include <stdint.h>

#include "hal_tof.h"

typedef struct {
    uint32_t t_ms;
    int8_t x;
    int8_t y;
    uint16_t bateria_mv;
    uint16_t tof_mm[TOF_QTD];
    uint8_t tof_ok;  /* bit i = 1 se o ToF i respondeu */
    uint8_t acao;    /* nav_acao_t da última decisão */
} evento_leitura_t;

#endif /* APP_EVENTOS_H */
