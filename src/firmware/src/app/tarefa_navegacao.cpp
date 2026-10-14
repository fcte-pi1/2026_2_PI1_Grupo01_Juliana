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
#include "telemetria_nav.h"

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              200  /* 5 Hz; a taxa da tel quem decide é a telemetria */
#define LIMIAR_PAREDE_MM        120  /* PROVISÓRIO: o FIRM-03 calibra e adiciona histerese */

static bool tem_parede(uint16_t distancia_mm, bool leitura_ok)
{
    return leitura_ok && distancia_mm < LIMIAR_PAREDE_MM;
}

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

    for (;;) {
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real. */
        sim_mundo_avancar(PERIODO_MS);
#endif

        uint16_t tof_mm[TOF_QTD] = {};
        uint8_t tof_ok = 0;  /* bit i = 1 se o ToF i respondeu */
        for (int i = 0; i < TOF_QTD; i++) {
            if (hal_tof_ler_mm((tof_id_t)i, &tof_mm[i])) {
                tof_ok |= (uint8_t)(1u << i);
            }
        }
        /* O alerta (LED + buzzer) e a falha low_battery usam bateria_nivel() quando a máquina de estados entrar. */
        bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());

        nav_leitura_t leitura;
        leitura.frente = tem_parede(tof_mm[TOF_FRONTAL_ESQ], tof_ok & (1u << TOF_FRONTAL_ESQ)) ||
                         tem_parede(tof_mm[TOF_FRONTAL_DIR], tof_ok & (1u << TOF_FRONTAL_DIR));
        leitura.esquerda = tem_parede(tof_mm[TOF_ESQUERDO], tof_ok & (1u << TOF_ESQUERDO));
        leitura.direita = tem_parede(tof_mm[TOF_DIREITO], tof_ok & (1u << TOF_DIREITO));

        /* A leitura é da célula atual: se ela mudou desde a última volta, vai um passo. */
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

        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(PERIODO_MS));
    }
}
