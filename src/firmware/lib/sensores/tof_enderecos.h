#ifndef TOF_ENDERECOS_H
#define TOF_ENDERECOS_H

/*
 * Sequência de inicialização dos 4 VL53L0X pelo XSHUT (FIRM-03, #129).
 *
 * Os 4 sensores saem de fábrica no mesmo endereço I²C (0x29). Para conviverem
 * no barramento, o firmware desliga todos pelo XSHUT e liga um por vez,
 * trocando o endereço de cada um antes de ligar o próximo.
 *
 * Esta função só DESCREVE os passos (C puro, testável no PC). O driver real
 * (FIRM-10, #141) percorre a lista chamando digitalWrite, delay e a troca de
 * endereço da biblioteca do sensor.
 */

#include <stdint.h>

#include "hal_tof.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TOF_ENDERECO_PADRAO 0x29
#define TOF_ENDERECO_BASE   0x30  /* sensor i fica em 0x30 + i */
#define TOF_ESPERA_XSHUT_MS 10    /* o VL53L0X leva ~1,2 ms para acordar; folga de sobra */

typedef enum {
    TOF_PASSO_XSHUT_BAIXO = 0, /* desliga o sensor (pino em nível baixo) */
    TOF_PASSO_XSHUT_ALTO,      /* liga o sensor */
    TOF_PASSO_ESPERAR_MS,
    TOF_PASSO_TROCAR_ENDERECO  /* do endereço padrão para TOF_ENDERECO_BASE + sensor */
} tof_passo_tipo_t;

typedef struct {
    tof_passo_tipo_t tipo;
    int8_t sensor;   /* tof_id_t; -1 em ESPERAR_MS */
    int8_t pino;     /* GPIO do XSHUT; -1 quando não se aplica */
    uint8_t valor;   /* ms em ESPERAR_MS, novo endereço em TROCAR_ENDERECO */
} tof_passo_t;

#define TOF_PASSOS_INICIO (TOF_QTD + 1 + TOF_QTD * 3)

/* GPIO do XSHUT de cada sensor (config/pinos.h). */
int8_t tof_pino_xshut(tof_id_t id);

/* Endereço I²C final do sensor. */
uint8_t tof_endereco(tof_id_t id);

/* Preenche a sequência e devolve o número de passos (TOF_PASSOS_INICIO). */
int tof_sequencia_inicio(tof_passo_t passos[TOF_PASSOS_INICIO]);

#ifdef __cplusplus
}
#endif

#endif /* TOF_ENDERECOS_H */
