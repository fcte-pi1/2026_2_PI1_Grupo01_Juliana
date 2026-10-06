/*
 * Tarefa de telemetria (núcleo 0): tira os eventos da fila e envia uma linha
 * JSON por mensagem para a ponte do notebook.
 *
 * O formato segue o que está descrito no ARQ-01 (#107), mas é PROVISÓRIO:
 * a serialização completa, o buffer de reenvio e a leitura do comando
 * "interromper" são o FIRM-07 (#123).
 */

#include <Arduino.h>
#include <stdio.h>

#include "app.h"
#include "eventos.h"
#include "hal_bt.h"
#include "hal_tempo.h"

#define VERSAO_CONTRATO     1
#define PERIODO_HEARTBEAT   1000
#define TAMANHO_MENSAGEM    256  /* limite do ARQ-01 */

static uint32_t seq;

static void enviar(const char *linha, int tamanho)
{
    if (tamanho > 0 && tamanho < TAMANHO_MENSAGEM) {
        hal_bt_escrever(linha, (size_t)tamanho);
    }
}

static void enviar_tel(const evento_leitura_t *ev)
{
    char linha[TAMANHO_MENSAGEM];
    int n = snprintf(linha, sizeof(linha),
                     "{\"v\":%d,\"seq\":%lu,\"t_ms\":%lu,\"tipo\":\"tel\","
                     "\"x\":%d,\"y\":%d,\"bat_mv\":%u,\"vel_mm_s\":0,"
                     "\"estado\":\"parado\",\"tipo_lab\":\"indeterminado\"}\n",
                     VERSAO_CONTRATO, (unsigned long)seq++, (unsigned long)ev->t_ms,
                     ev->x, ev->y, ev->bateria_mv);
    enviar(linha, n);

    /* Linha de depuração (começa com '#', a ponte ignora): leituras cruas dos ToF. */
    n = snprintf(linha, sizeof(linha), "# tof_mm fe=%u fd=%u e=%u d=%u ok=0x%X acao=%u\n",
                 ev->tof_mm[TOF_FRONTAL_ESQ], ev->tof_mm[TOF_FRONTAL_DIR],
                 ev->tof_mm[TOF_ESQUERDO], ev->tof_mm[TOF_DIREITO], ev->tof_ok, ev->acao);
    enviar(linha, n);
}

static void enviar_heartbeat(void)
{
    char linha[TAMANHO_MENSAGEM];
    int n = snprintf(linha, sizeof(linha),
                     "{\"v\":%d,\"seq\":%lu,\"t_ms\":%lu,\"tipo\":\"heartbeat\"}\n",
                     VERSAO_CONTRATO, (unsigned long)seq++, (unsigned long)hal_tempo_ms());
    enviar(linha, n);
}

void tarefa_telemetria(void *parametro)
{
    (void)parametro;
    uint32_t ultimo_heartbeat = 0;
    evento_leitura_t ev;

    for (;;) {
        /* Espera um evento por até 100 ms; sem evento, só confere o heartbeat. */
        if (xQueueReceive(fila_eventos, &ev, pdMS_TO_TICKS(100)) == pdTRUE) {
            enviar_tel(&ev);
        }

        uint32_t agora = hal_tempo_ms();
        if (agora - ultimo_heartbeat >= PERIODO_HEARTBEAT) {
            ultimo_heartbeat = agora;
            enviar_heartbeat();
        }
    }
}
