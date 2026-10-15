#ifndef SENSORES_H
#define SENSORES_H

/*
 * Leitura contínua dos 3 ToF (FIRM-03, #129): lê pela HAL, filtra e detecta
 * paredes. Funciona igual com a HAL simulada e com a real (FIRM-10, #141).
 *
 * Uso na tarefa de navegação, a cada 50 ms (20 Hz):
 *     sensores_ler();
 *     paredes_t p = sensores_paredes();
 */

#include <stdbool.h>
#include <stdint.h>

#include "hal_tof.h"
#include "paredes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Zera filtros, falhas e paredes (início de tentativa). */
void sensores_iniciar(void);

/* Lê os 3 sensores uma vez e atualiza filtros e paredes. */
void sensores_ler(void);

paredes_t sensores_paredes(void);

/* Distância filtrada em mm; false se o sensor falhou ou ainda não leu. */
bool sensores_distancia_mm(tof_id_t id, uint16_t *distancia_mm);

/* true depois de TOF_FALHAS_PARA_DESCARTAR leituras seguidas sem resposta. */
bool sensores_tof_falhou(tof_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* SENSORES_H */
