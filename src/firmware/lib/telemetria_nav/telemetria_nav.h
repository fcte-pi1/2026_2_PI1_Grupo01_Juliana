#ifndef TELEMETRIA_NAV_H
#define TELEMETRIA_NAV_H

/*
 * Conversões da navegação para o contrato — FIRM-07 (#123).
 *
 * A navegação conta as direções em N/L/S/O (nav_direcao_t) e vê as paredes
 * relativas à frente do robô; o contrato usa N/S/L/O e paredes absolutas
 * (N = +y, S = -y, L = +x, O = -x). Ficam aqui, e não em lib/navegacao, para
 * a navegação não depender do contrato, e numa lib própria para não puxar a
 * HAL de lib/telemetria. C puro.
 */

#include <stdint.h>

#include "nav.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "telemetria.h" /* contrato v1, em lib/contrato */

contrato_rumo_t telemetria_rumo_do_nav(nav_direcao_t direcao);

/*
 * Máscara absoluta (soma de CONTRATO_PAREDE_*) das paredes vistas virado para
 * direcao. A parede de trás não é lida e fica fora da máscara.
 */
uint8_t telemetria_paredes_do_nav(nav_direcao_t direcao, nav_leitura_t leitura);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRIA_NAV_H */
