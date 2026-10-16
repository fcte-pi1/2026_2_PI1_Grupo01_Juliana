/*
 * Tarefa de navegação (núcleo 1): lê os sensores, pede a decisão à navegação
 * e publica para a telemetria a amostra da tel e os eventos do contrato.
 *
 * Por enquanto o robô não se move (as primitivas de movimento são o FIRM-05)
 * e a navegação ainda não decide nada (FIRM-01). O laço já mostra o caminho
 * completo: sensores -> navegação -> filas -> telemetria.
 */

#include <Arduino.h>

#include "app.h"
#include "bateria.h"
#include "config/robo.h"
#include "eventos.h"
#include "hal_bateria.h"
#include "hal_led.h"
#include "hal_tempo.h"
#include "hal_tof.h"
#include "nav.h"
#include "sensores.h"
#include "telemetria_nav.h"

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              50   /* 20 Hz: leitura dos ToF (FIRM-03) */
#define CICLOS_POR_DECISAO      4    /* a navegação decide a 5 Hz; a taxa da tel quem decide é a telemetria */

static void publicar_passo(const nav_estado_t *nav, nav_leitura_t leitura)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_PASSO;
    ev.m.passo.x = (uint8_t)nav->x;
    ev.m.passo.y = (uint8_t)nav->y;
    ev.m.passo.paredes = telemetria_paredes_do_nav(nav->direcao, leitura);
    app_publicar_evento(&ev);
}

void tarefa_navegacao(void *parametro)
{
    (void)parametro;
    nav_estado_t nav;
    nav_iniciar(&nav);
    bateria_t bateria;
    bateria_iniciar(&bateria);

    /* Célula do último passo publicado; a largada não gera passo. */
    int8_t ultimo_x = nav.x;
    int8_t ultimo_y = nav.y;

    TickType_t proximo = xTaskGetTickCount();
    bool led = false;
    int ciclo = 0;
#ifdef TELEMETRIA_DEPURACAO
    uint32_t volta_anterior = hal_tempo_ms();
#endif

    for (;;) {
#ifdef TELEMETRIA_DEPURACAO
        uint32_t inicio_volta = hal_tempo_ms();
        if (inicio_volta - volta_anterior > app_nav_periodo_max_ms) {
            app_nav_periodo_max_ms = inicio_volta - volta_anterior;
        }
        volta_anterior = inicio_volta;
#endif
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real. */
        sim_mundo_avancar(PERIODO_MS);
#endif

        /* Os ToF são lidos a cada ciclo (20 Hz) para o filtro ter amostras novas. */
        sensores_ler();

#ifdef TELEMETRIA_DEPURACAO
        for (int i = 0; i < TOF_QTD; i++) {
            uint16_t mm = 0;
            if (sensores_distancia_mm((tof_id_t)i, &mm)) {
                app_tof_ok = (uint8_t)(app_tof_ok | (1u << i));
            } else {
                app_tof_ok = (uint8_t)(app_tof_ok & ~(1u << i));
            }
            app_tof_mm[i] = mm;
        }
#endif

        if (++ciclo >= CICLOS_POR_DECISAO) {
            ciclo = 0;

            /* O alerta (LED + buzzer) e a falha low_battery usam bateria_nivel() quando a máquina de estados entrar. */
            bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());

            /*
             * A navegação decide por célula; até o FIRM-05 (#135) não há
             * movimento, então ela continua consultada a 5 Hz, como antes.
             */
            paredes_t paredes = sensores_paredes();
            nav_leitura_t leitura;
            leitura.frente = paredes.frente;
            leitura.esquerda = paredes.esquerda;
            leitura.direita = paredes.direita;

            /* A leitura é da célula atual: se ela mudou desde a última decisão, vai um passo. */
            if (nav.x != ultimo_x || nav.y != ultimo_y) {
                publicar_passo(&nav, leitura);
                ultimo_x = nav.x;
                ultimo_y = nav.y;
            }

            nav_acao_t acao = nav_passo(&nav, leitura);

            /* Campos que ainda não existem (FIRM-01 e máquina de estados): valores fixos. */
            telemetria_amostra_t amostra = {};
            amostra.tel.estado = CONTRATO_ESTADO_HEALTH_CHECK;
            amostra.tel.x = (uint8_t)nav.x;
            amostra.tel.y = (uint8_t)nav.y;
            amostra.tel.rumo = telemetria_rumo_do_nav(nav.direcao);
            amostra.tel.bat_mv = bateria_mv(&bateria);
            amostra.tel.vel_mm_s = 0;
            amostra.tel.eixo_longo = CONTRATO_EIXO_DESCONHECIDO;
            amostra.em_movimento = acao != NAV_ACAO_SUCESSO && acao != NAV_ACAO_SEM_PASSO;
            app_publicar_amostra(&amostra);

            led = !led;
            hal_led_definir(led);
        }

        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(PERIODO_MS));
    }
}
