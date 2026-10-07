#ifndef TOF_AUTOTESTE_H
#define TOF_AUTOTESTE_H

/*
 * Autoteste dos 4 ToF para o health-check (FIRM-03, #129; usado pela FIRM-09, #138).
 *
 * Cada sensor faz TOF_AUTOTESTE_AMOSTRAS leituras. É aprovado se todas
 * responderam e a mediana (saturada no alcance) é >= TOF_AUTOTESTE_MIN_MM.
 * Os campos seguem o hc_item do contrato (4.4.1, Tabelas 3 e 4):
 * componente = nome, aprovado, valor = mediana em mm ou null se nada respondeu.
 */

#include <stdbool.h>
#include <stdint.h>

#include "hal_tof.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *nome;  /* ex.: "tof_esquerdo" (hal_tof_nome) */
    bool aprovado;
    bool tem_valor;    /* false: nenhuma leitura respondeu (valor vai como null) */
    uint16_t valor_mm; /* mediana das leituras que responderam */
} tof_autoteste_t;

/*
 * Avalia as amostras de um sensor (n de 1 a TOF_AUTOTESTE_AMOSTRAS; o excesso é
 * ignorado). Devolve se foi aprovado; *tem_valor e *valor_mm dizem o que medir.
 * C puro: os testes passam valores sintéticos.
 */
bool tof_autoteste_avaliar(const bool respondeu[], const uint16_t distancia_mm[], int n,
                           bool *tem_valor, uint16_t *valor_mm);

/* Lê os 4 sensores pela HAL e preenche um resultado por sensor (ordem de tof_id_t). */
void tof_autoteste_executar(tof_autoteste_t resultado[TOF_QTD]);

#ifdef __cplusplus
}
#endif

#endif /* TOF_AUTOTESTE_H */
