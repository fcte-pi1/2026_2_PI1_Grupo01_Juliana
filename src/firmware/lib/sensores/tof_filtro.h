#ifndef TOF_FILTRO_H
#define TOF_FILTRO_H

/*
 * Filtro de um sensor ToF (FIRM-03, #129): mediana das últimas leituras e
 * contagem de falhas.
 *
 * C puro, sem Arduino e sem HAL: recebe as leituras como parâmetro, então os
 * testes alimentam sequências sintéticas.
 *
 * Regras:
 *   - leitura válida acima do alcance (ex.: 8190, "nada à vista") é saturada
 *     em TOF_ALCANCE_MAX_MM: corredor aberto, não falha;
 *   - leitura sem resposta não entra na mediana; depois de
 *     TOF_FALHAS_PARA_DESCARTAR seguidas, o sensor fica marcado como falho
 *     até tof_filtro_iniciar. Quem gera a falha_componente é a FIRM-09 (#138).
 */

#include <stdbool.h>
#include <stdint.h>

#include "config/robo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t amostras[TOF_MEDIANA_AMOSTRAS];
    uint8_t quantidade;      /* amostras válidas guardadas (até a janela) */
    uint8_t proxima;         /* posição circular da próxima amostra */
    uint8_t falhas_seguidas;
    bool falhou;
} tof_filtro_t;

void tof_filtro_iniciar(tof_filtro_t *f);

/* respondeu = false quando a HAL não conseguiu ler o sensor. */
void tof_filtro_inserir(tof_filtro_t *f, bool respondeu, uint16_t distancia_mm);

/* true se o sensor não falhou e já tem ao menos uma leitura. */
bool tof_filtro_valido(const tof_filtro_t *f);

/* Mediana das amostras guardadas, em mm. Só tem sentido se tof_filtro_valido. */
uint16_t tof_filtro_mediana(const tof_filtro_t *f);

/* Limita a distância ao alcance do sensor. */
uint16_t tof_saturar(uint16_t distancia_mm);

/* Mediana de n valores (n >= 1); não altera o vetor. */
uint16_t tof_mediana(const uint16_t *valores, int n);

#ifdef __cplusplus
}
#endif

#endif /* TOF_FILTRO_H */
