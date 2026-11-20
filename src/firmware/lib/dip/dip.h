#ifndef DIP_H
#define DIP_H

/*
 * Tipo do labirinto pelo DIP switch.
 *
 * As vias 1 e 2 formam o código (via 2 é o bit mais alto):
 *   00 = 4x4,  01 = 8x4,  10 = 12x4,  11 = inválido (reprova o health-check).
 * As vias 3 e 4 são de outros modos e não mudam o tipo.
 *
 * C puro: recebe as vias já lidas (hal_dip_ler), roda igual na ESP32 e nos testes.
 */

#include <stdint.h>

#include "nav.h"
#include "telemetria.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tipo do contrato para o hc_resultado; CONTRATO_DIP_INVALIDO para o código 11. */
contrato_tipo_dip_t dip_tipo(uint8_t vias);

/* Tipo para a navegação e a memória. O código 11 devolve 4x4, mas o health-check reprova. */
nav_tipo_t dip_tipo_nav(uint8_t vias);

#ifdef __cplusplus
}
#endif

#endif /* DIP_H */
