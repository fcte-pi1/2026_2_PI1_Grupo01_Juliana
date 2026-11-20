/*
 * Tarefa de navegação (núcleo 1): lê os sensores, pede a decisão à navegação
 * e publica para a telemetria a amostra da tel e os eventos do contrato.
 *
 * Com a HAL simulada, o robô é colocado na célula em que a navegação entrou,
 * na faixa em que o ToF lateral mede essa célula. O laço mostra o caminho
 * completo: sensores -> navegação -> NVS -> filas -> telemetria.
 *
 * No boot, carrega o mapa da NVS (FIRM-08), publica o health-check (hc_item e
 * hc_resultado, com inicio "retomada" quando o mapa voltou) e, a cada célula
 * nova, grava de novo. Sucesso e falha saem uma vez, quando a corrida acaba.
 */

#include <Arduino.h>

#include "app.h"
#include "bateria.h"
#include "config/robo.h"
#include "eventos.h"
#include "dip.h"
#include "hal_bateria.h"
#include "hal_dip.h"
#include "hal_led.h"
#include "hal_motor.h"
#include "hal_tempo.h"
#include "hal_tof.h"
#include "health_check.h"
#include "memoria.h"
#include "movimento.h"
#include "nav.h"
#include "sensores.h"
#include "telemetria_nav.h"

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              50   /* 20 Hz: leitura dos ToF (FIRM-03) */
#define CICLOS_POR_DECISAO      4    /* a navegação decide a 5 Hz; a taxa da tel quem decide é a telemetria */

static void publicar_falha(contrato_motivo_t motivo, int8_t x, int8_t y,
                           contrato_componente_t componente)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_FALHA;
    ev.m.falha.motivo = motivo;
    ev.m.falha.origem = CONTRATO_ORIGEM_AUTOMATICA;
    ev.m.falha.x = (uint8_t)x;
    ev.m.falha.y = (uint8_t)y;
    ev.m.falha.componente = componente;
    app_publicar_evento(&ev);
}

static void publicar_sucesso(int8_t x, int8_t y)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_SUCESSO;
    ev.m.sucesso.x = (uint8_t)x;
    ev.m.sucesso.y = (uint8_t)y;
    app_publicar_evento(&ev);
}

static void publicar_passo(int8_t x, int8_t y, uint8_t paredes)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_PASSO;
    ev.m.passo.x = (uint8_t)x;
    ev.m.passo.y = (uint8_t)y;
    ev.m.passo.paredes = paredes;
    app_publicar_evento(&ev);
}

/* Espera do autoteste dos motores: a tarefa dorme, e a simulação deixa o mundo andar. */
static void esperar_ms(uint32_t ms)
{
#ifdef HAL_SIMULADA
    for (uint32_t t = 0; t < ms; t += PERIODO_MS) {
        sim_mundo_avancar(PERIODO_MS);
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MS));
    }
#else
    vTaskDelay(pdMS_TO_TICKS(ms));
#endif
}

/*
 * Roda o health-check (8 itens) e publica um hc_item por componente e o
 * hc_resultado. Reprovação não gera falha: vai só nesses eventos.
 * Devolve se foi aprovado.
 */
static bool publicar_health_check(mem_inicio_t inicio, bateria_t *bateria)
{
    hc_resultado_t resultado;
    mov_parar(); /* o autoteste gira as rodas: o movimento tem que estar parado */
#ifdef HAL_SIMULADA
    sim_definir_rodas_no_ar(true); /* como no robô real durante o autoteste */
#endif
    hc_executar(bateria, esperar_ms, &resultado);
#ifdef HAL_SIMULADA
    sim_definir_rodas_no_ar(false);
#endif
    telemetria_evento_t eventos[HC_EVENTOS];
    int n = hc_montar_eventos(&resultado,
                              inicio == MEM_RETOMADA ? CONTRATO_INICIO_RETOMADA : CONTRATO_INICIO_NOVA,
                              eventos);
    for (int i = 0; i < n; i++) {
        app_publicar_evento(&eventos[i]);
    }
    return resultado.aprovado;
}

#ifdef HAL_SIMULADA
static void acompanhar_celula(const nav_estado_t *nav)
{
    sim_mundo_na_celula(nav->x, nav->y, (int)nav->direcao);
}
#endif

void tarefa_navegacao(void *parametro)
{
    (void)parametro;
    /* Estático: com cerca de 1,2 KB, o estado não deve ocupar a pilha de 4 KB da tarefa. */
    static nav_estado_t nav;
    contrato_estado_t estado = CONTRATO_ESTADO_HEALTH_CHECK;
    mem_inicio_t inicio = mem_carregar(&nav, &estado, dip_tipo_nav(hal_dip_ler()));
    bateria_t bateria;
    bateria_iniciar(&bateria);

#ifdef HAL_SIMULADA
    acompanhar_celula(&nav);
#endif

    bool hc_ok = publicar_health_check(inicio, &bateria);
    if (!hc_ok) {
        /* Sem falha: o robô só fica parado em health-check até um novo health-check aprovado. */
        estado = CONTRATO_ESTADO_HEALTH_CHECK;
    } else if (estado == CONTRATO_ESTADO_HEALTH_CHECK) {
        estado = CONTRATO_ESTADO_RUNNING;
    }

    /* Célula do último passo publicado; a largada não gera passo. */
    int8_t ultimo_x = nav.x;
    int8_t ultimo_y = nav.y;
    if (inicio == MEM_RETOMADA) {
        /* A retomada começa na célula da falha e ganha o próprio passo: o backend o marca como retomada. */
        ultimo_x = -1;
        ultimo_y = -1;
    }
    /* Célula já gravada na NVS; começa na que o boot carregou. */
    int8_t gravada_x = nav.x;
    int8_t gravada_y = nav.y;

    TickType_t proximo = xTaskGetTickCount();
    bool led = false;
    int ciclo = 0;
#ifdef TELEMETRIA_DEPURACAO
    uint32_t volta_anterior = hal_tempo_ms();
#endif
    bool terminou = nav.terminou || !hc_ok;
    nav_acao_t acao = terminou ? nav.acao_terminal : NAV_ACAO_SEM_PASSO;

    for (;;) {
#ifdef TELEMETRIA_DEPURACAO
        uint32_t inicio_volta = hal_tempo_ms();
        if (inicio_volta - volta_anterior > app_nav_periodo_max_ms) {
            app_nav_periodo_max_ms = inicio_volta - volta_anterior;
        }
        volta_anterior = inicio_volta;
#endif
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real. Sem PWM, a pose não muda. */
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

            bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
            if (bateria_pronta(&bateria)) {
                hal_motor_definir_tensao(bateria_ponte_mv(&bateria));
            }

            paredes_t paredes = sensores_paredes();
            nav_leitura_t leitura;
            leitura.frente = paredes.frente;
            leitura.esquerda = paredes.esquerda;
            leitura.direita = paredes.direita;

            /* A leitura é da célula atual; a pose é guardada antes de o passo levá-la à próxima. */
            int8_t x = nav.x;
            int8_t y = nav.y;
            nav_direcao_t direcao = nav.direcao;

            bool ja_acabou = terminou;
            acao = mem_aplicar_leitura(&nav, &estado, &gravada_x, &gravada_y, &terminou, leitura);

#ifdef HAL_SIMULADA
            if (nav.x != x || nav.y != y || nav.direcao != direcao) {
                acompanhar_celula(&nav);
            }
#endif

            /* O passo da célula lida vem antes de sucesso ou falha, com o lado que o passo fixou. */
            telemetria_decisao_t decisao = {};
            decisao.mudou_celula = x != ultimo_x || y != ultimo_y;
            decisao.x = x;
            decisao.y = y;
            decisao.paredes = telemetria_paredes_do_nav(direcao, nav.lado, leitura);
            decisao.terminou_agora = !ja_acabou && terminou;
            decisao.acao = acao;
            decisao.nav_x = nav.x;
            decisao.nav_y = nav.y;
            telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX];
            int qtd = telemetria_eventos_da_decisao(&decisao, saida);
            for (int i = 0; i < qtd; i++) {
                switch (saida[i].tipo) {
                case TELEMETRIA_DECISAO_PASSO:
                    publicar_passo((int8_t)saida[i].x, (int8_t)saida[i].y, saida[i].paredes);
                    break;
                case TELEMETRIA_DECISAO_SUCESSO:
                    publicar_sucesso((int8_t)saida[i].x, (int8_t)saida[i].y);
                    break;
                case TELEMETRIA_DECISAO_SEM_SAIDA:
                    publicar_falha(CONTRATO_MOTIVO_STUCK, (int8_t)saida[i].x, (int8_t)saida[i].y,
                                   CONTRATO_COMPONENTE_NENHUM);
                    break;
                }
            }
            if (decisao.mudou_celula) {
                ultimo_x = x;
                ultimo_y = y;
            }

            telemetria_amostra_t amostra = {};
            amostra.tel.estado = estado;
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
