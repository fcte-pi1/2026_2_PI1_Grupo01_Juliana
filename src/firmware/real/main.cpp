/*
 * Firmware com a HAL real, só com o health-check, por enquanto.
 *
 * Roda o health-check ao ligar e a cada comando health_check da ponte, e envia os hc_item,
 * o hc_resultado e a tel (estado health-check, a 1 Hz) pelo contrato v1. Não anda: os
 * motores só giram no autoteste, com as rodas no ar. Os comandos largar e interromper são
 * lidos e ignorados.
 *
 * O enlace é a serial USB (lib/hal/esp32/hal_bt_serial_esp32.cpp), então a ponte do
 * notebook abre /dev/ttyACM0.
 *
 *   pio run -e esp32_real -t upload             ToF real
 *   pio run -e esp32_real_tof_mock -t upload    ToF mockado (o health-check aprova)
 */

#include <Arduino.h>

#include "bateria.h"
#include "config/robo.h"
#include "hal_bateria.h"
#include "hal_bt.h"
#include "hal_dip.h"
#include "hal_encoder.h"
#include "hal_motor.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "health_check.h"
#include "telemetria_envio.h"

#define PERIODO_LACO_MS      5
#define PERIODO_BATERIA_MS   200

static bateria_t bateria;
static telemetria_amostra_t amostra;

/* A tel continua saindo durante o autoteste dos motores. */
static void esperar(uint32_t ms)
{
    uint32_t fim = millis() + ms;
    while ((int32_t)(millis() - fim) < 0) {
        telemetria_passo(millis(), &amostra);
        delay(PERIODO_LACO_MS);
    }
}

static void rodar_health_check(void)
{
    hc_resultado_t resultado;
    hc_executar(&bateria, esperar, &resultado);

    telemetria_evento_t eventos[HC_EVENTOS];
    int n = hc_montar_eventos(&resultado, CONTRATO_INICIO_NOVA, eventos);
    for (int i = 0; i < n; i++) {
        telemetria_inserir_evento(&eventos[i]);
    }
    amostra.tel.bat_mv = bateria_mv(&bateria);
}

void setup()
{
    Serial.begin(115200);
    hal_motor_iniciar(); /* motores primeiro: parados e em standby */
    hal_encoder_iniciar();
    hal_bateria_iniciar();
    hal_dip_iniciar();
    hal_nvs_iniciar();
    hal_bt_iniciar("Ratatouille");
    hal_tof_iniciar();
    bateria_iniciar(&bateria);

    telemetria_iniciar(telemetria_proximo_boot());

    amostra = {};
    amostra.tel.estado = CONTRATO_ESTADO_HEALTH_CHECK;
    amostra.tel.rumo = CONTRATO_RUMO_N;
    amostra.tel.eixo_longo = CONTRATO_EIXO_DESCONHECIDO;
    amostra.em_movimento = false;

    rodar_health_check();
}

void loop()
{
    static uint32_t ultima_bateria_ms;
    uint32_t agora = millis();
    if (agora - ultima_bateria_ms >= PERIODO_BATERIA_MS) {
        ultima_bateria_ms = agora;
        bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
        amostra.tel.bat_mv = bateria_mv(&bateria);
    }

    telemetria_passo(agora, &amostra);

    if (telemetria_consumir_health_check()) {
        rodar_health_check();
    }
    telemetria_consumir_largada();
    telemetria_consumir_interrupcao();
    delay(PERIODO_LACO_MS);
}
