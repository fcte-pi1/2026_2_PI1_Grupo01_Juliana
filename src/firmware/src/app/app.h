#ifndef APP_H
#define APP_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

/* Fila de evento_leitura_t, da navegação (núcleo 1) para a telemetria (núcleo 0). */
extern QueueHandle_t fila_eventos;

void app_iniciar(void);

void tarefa_navegacao(void *parametro);
void tarefa_telemetria(void *parametro);

#endif /* APP_H */
