#ifndef APP_H
#define APP_H

#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "eventos.h"

/* Da navegação (núcleo 1) para a telemetria (núcleo 0). */
extern QueueHandle_t fila_amostra;  /* telemetria_amostra_t, tamanho 1 */
extern QueueHandle_t fila_eventos;  /* telemetria_evento_t */

/* Contador de inicializações (NVS), lido uma vez em app_iniciar. */
extern uint32_t app_boot;

/*
 * Eventos perdidos por fila_eventos cheia. Só a navegação escreve; um
 * volatile de 32 bits basta para a telemetria ler de outro núcleo.
 */
extern volatile uint32_t app_eventos_descartados;

#ifdef TELEMETRIA_DEPURACAO
/*
 * Maior intervalo entre duas voltas do laço da navegação, em ms. A navegação
 * só aumenta e a telemetria zera a cada linha "#"; um máximo perdido na
 * corrida entre as duas só afeta a depuração.
 */
extern volatile uint32_t app_nav_periodo_max_ms;
#endif

void app_iniciar(void);

/* Publicam para a telemetria sem nunca esperar. */
void app_publicar_amostra(const telemetria_amostra_t *amostra);
void app_publicar_evento(const telemetria_evento_t *evento);

void tarefa_navegacao(void *parametro);
void tarefa_telemetria(void *parametro);

#endif /* APP_H */
