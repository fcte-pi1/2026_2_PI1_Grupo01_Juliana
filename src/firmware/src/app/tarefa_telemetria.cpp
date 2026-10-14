/*
 * Tarefa de telemetria (núcleo 0): liga as filas da navegação à
 * lib/telemetria, que serializa o contrato v1 (ARQ-01, #107), guarda os
 * eventos para reenvio e lê o comando interromper.
 *
 * A tarefa espera na fila de eventos até o próximo prazo devolvido por
 * telemetria_passo (no máximo 50 ms), então um evento novo a acorda na hora.
 */

#include <Arduino.h>

#include "app.h"
#include "eventos.h"
#include "hal_tempo.h"

#ifdef TELEMETRIA_DEPURACAO
#include <stdio.h>

#include "hal_bt.h"

#define PERIODO_DEPURACAO_MS 5000

/* Linha "#" que o backend e o verificar_serial.py ignoram. */
static void enviar_depuracao(void)
{
    char linha[96];
    uint32_t nav_max = app_nav_periodo_max_ms;
    app_nav_periodo_max_ms = 0;
    int n = snprintf(linha, sizeof(linha),
                     "# tel desc_fila=%lu desc_buffer=%lu nav_periodo_max_ms=%lu\n",
                     (unsigned long)app_eventos_descartados,
                     (unsigned long)telemetria_descartados(), (unsigned long)nav_max);
    if (n > 0 && (size_t)n < sizeof(linha)) {
        hal_bt_escrever(linha, (size_t)n);
    }
}
#endif

void tarefa_telemetria(void *parametro)
{
    (void)parametro;
    telemetria_iniciar(app_boot);
    uint32_t espera_ms = TELEMETRIA_PERIODO_EVENTO_MS;
    telemetria_evento_t ev;
    telemetria_amostra_t amostra;
#ifdef TELEMETRIA_DEPURACAO
    uint32_t ultima_depuracao = hal_tempo_ms();
#endif

    for (;;) {
        if (xQueueReceive(fila_eventos, &ev, pdMS_TO_TICKS(espera_ms)) == pdTRUE) {
            do {
                telemetria_inserir_evento(&ev);
            } while (xQueueReceive(fila_eventos, &ev, 0) == pdTRUE);
        }

        /* Antes da primeira amostra a fila está vazia e nenhuma tel sai. */
        bool tem_amostra = xQueuePeek(fila_amostra, &amostra, 0) == pdTRUE;
        uint32_t agora = hal_tempo_ms();
        espera_ms = telemetria_passo(agora, tem_amostra ? &amostra : NULL);

#ifdef TELEMETRIA_DEPURACAO
        if (agora - ultima_depuracao >= PERIODO_DEPURACAO_MS) {
            ultima_depuracao = agora;
            enviar_depuracao();
        }
#endif
    }
}
