#ifndef HAL_TOF_H
#define HAL_TOF_H

/* Sensores de distância Time-of-Flight (VL53L0X) — FIRM-03 (#129) */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOF_FRONTAL = 0,
    TOF_ESQUERDO,
    TOF_DIREITO,
    TOF_QTD
} tof_id_t;

/* Nome usado no health-check e na telemetria (ex.: "tof_esquerdo"). */
const char *hal_tof_nome(tof_id_t id);

/* Liga os 3 sensores. Retorna false se algum não respondeu. */
bool hal_tof_iniciar(void);

/* Lê a distância em mm. Retorna false se o sensor não respondeu. */
bool hal_tof_ler_mm(tof_id_t id, uint16_t *distancia_mm);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TOF_H */
