/*
 * Tarefa de navegação (núcleo 1): lê os sensores, pede a decisão à navegação
 * e manda os dados para a telemetria pela fila.
 *
 * Por enquanto o robô não se move (as primitivas de movimento são o FIRM-05)
 * e a navegação ainda não decide nada (FIRM-01). O laço já mostra o caminho
 * completo: sensores -> navegação -> fila -> telemetria.
 */

#include <Arduino.h>

#include "app.h"
#include "config/robo.h"
#include "eventos.h"
#include "hal_bateria.h"
#include "hal_led.h"
#include "hal_tempo.h"
#include "hal_tof.h"
#include "nav.h"
#include "sensores.h"

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              50   /* 20 Hz: leitura dos ToF (FIRM-03) */
#define CICLOS_POR_EVENTO       4    /* 1 evento a cada 4 ciclos: "tel" a 5 Hz (ARQ-01) */

void tarefa_navegacao(void *parametro)
{
    (void)parametro;
    nav_estado_t nav;
    nav_iniciar(&nav);

    TickType_t proximo = xTaskGetTickCount();
    bool led = false;
    int ciclo = 0;

    for (;;) {
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real. */
        sim_mundo_avancar(PERIODO_MS);
#endif

        /* Os ToF são lidos a cada ciclo (20 Hz) para o filtro ter amostras novas. */
        sensores_ler();

        if (++ciclo >= CICLOS_POR_EVENTO) {
            ciclo = 0;

            /*
             * A navegação decide por célula; até o FIRM-05 (#135) não há
             * movimento, então ela continua consultada a 5 Hz, como antes.
             */
            paredes_t paredes = sensores_paredes();
            nav_leitura_t leitura;
            leitura.frente = paredes.frente;
            leitura.esquerda = paredes.esquerda;
            leitura.direita = paredes.direita;
            nav_acao_t acao = nav_passo(&nav, leitura);

            evento_leitura_t ev = {};
            ev.t_ms = hal_tempo_ms();
            for (int i = 0; i < TOF_QTD; i++) {
                if (sensores_distancia_mm((tof_id_t)i, &ev.tof_mm[i])) {
                    ev.tof_ok |= (uint8_t)(1u << i);
                }
            }
            ev.bateria_mv = (uint16_t)(hal_bateria_ler_adc_mv() * BATERIA_DIVISOR);
            ev.acao = (uint8_t)acao;
            ev.x = nav.x;
            ev.y = nav.y;

            /* Fila cheia: descarta em vez de esperar, a navegação nunca trava. */
            xQueueSend(fila_eventos, &ev, 0);

            led = !led;
            hal_led_definir(led);
        }

        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(PERIODO_MS));
    }
}
