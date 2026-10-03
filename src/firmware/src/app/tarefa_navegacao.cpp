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

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              200  /* 5 Hz, a taxa do "tel" no ARQ-01 */
#define LIMIAR_PAREDE_MM        120  /* PROVISÓRIO: o FIRM-03 calibra e adiciona histerese */

static bool tem_parede(uint16_t distancia_mm, bool leitura_ok)
{
    return leitura_ok && distancia_mm < LIMIAR_PAREDE_MM;
}

void tarefa_navegacao(void *parametro)
{
    (void)parametro;
    nav_estado_t nav;
    nav_iniciar(&nav);

    TickType_t proximo = xTaskGetTickCount();
    bool led = false;

    for (;;) {
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real. */
        sim_mundo_avancar(PERIODO_MS);
#endif

        evento_leitura_t ev = {};
        ev.t_ms = hal_tempo_ms();

        for (int i = 0; i < TOF_QTD; i++) {
            if (hal_tof_ler_mm((tof_id_t)i, &ev.tof_mm[i])) {
                ev.tof_ok |= (uint8_t)(1u << i);
            }
        }
        ev.bateria_mv = (uint16_t)(hal_bateria_ler_adc_mv() * BATERIA_DIVISOR);

        nav_leitura_t leitura;
        leitura.frente = tem_parede(ev.tof_mm[TOF_FRONTAL_ESQ], ev.tof_ok & (1u << TOF_FRONTAL_ESQ)) ||
                         tem_parede(ev.tof_mm[TOF_FRONTAL_DIR], ev.tof_ok & (1u << TOF_FRONTAL_DIR));
        leitura.esquerda = tem_parede(ev.tof_mm[TOF_ESQUERDO], ev.tof_ok & (1u << TOF_ESQUERDO));
        leitura.direita = tem_parede(ev.tof_mm[TOF_DIREITO], ev.tof_ok & (1u << TOF_DIREITO));

        ev.acao = (uint8_t)nav_passo(&nav, leitura);
        ev.x = nav.x;
        ev.y = nav.y;

        /* Fila cheia: descarta em vez de esperar, a navegação nunca trava. */
        xQueueSend(fila_eventos, &ev, 0);

        led = !led;
        hal_led_definir(led);

        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(PERIODO_MS));
    }
}
