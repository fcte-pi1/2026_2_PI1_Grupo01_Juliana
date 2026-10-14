#ifndef APP_EVENTOS_H
#define APP_EVENTOS_H

/*
 * Mensagens da tarefa de navegação para a de telemetria, pelas filas do
 * FreeRTOS. A navegação só publica e segue em frente: ela nunca espera o
 * Bluetooth (RNF07).
 *
 *   - telemetria_amostra_t: os campos da tel mais em_movimento; a fila tem
 *     tamanho 1 e a amostra nova sobrescreve a anterior;
 *   - telemetria_evento_t: hc_item, hc_resultado, passo, falha e sucesso,
 *     que a telemetria guarda no buffer de reenvio.
 */

/* Caminho relativo: lib/contrato também tem um telemetria.h. */
#include "../../lib/telemetria/telemetria.h"

#endif /* APP_EVENTOS_H */
