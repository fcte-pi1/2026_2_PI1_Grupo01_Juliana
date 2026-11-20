/*
 * Firmware com a HAL real, só com o health-check, por enquanto.
 *
 * Roda o health-check ao ligar e a cada comando health_check da ponte, e envia os hc_item,
 * o hc_resultado e a tel (estado health-check, a 1 Hz) pelo contrato v1. Não anda: os
 * motores só giram no autoteste, com as rodas no ar. Os comandos largar e interromper são
 * lidos e ignorados.
 *
 * O botão BOOT passa pela máquina de estados: toque curto apaga o mapa da NVS; toque longo
 * (BOTAO_LONGO_MS, com bipe duplo) apaga também os tipos resolvidos. A serial mostra, em linhas
 * com "#", quais chaves existem na NVS antes e depois (a ponte não as envia ao back-end).
 *
 * O enlace é a serial USB (lib/hal/esp32/hal_bt_serial_esp32.cpp), então a ponte do
 * notebook abre /dev/ttyACM0.
 *
 *   pio run -e esp32_real -t upload             ToF real
 *   pio run -e esp32_real_tof_mock -t upload    ToF mockado (o health-check aprova)
 */

#include <Arduino.h>

#include "bateria.h"
#include "botao.h"
#include "config/robo.h"
#include "hal_bateria.h"
#include "hal_boot.h"
#include "hal_bt.h"
#include "hal_dip.h"
#include "hal_encoder.h"
#include "hal_led.h"
#include "hal_motor.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "health_check.h"
#include "maquina.h"
#include "memoria.h"
#include "telemetria_envio.h"

#define PERIODO_LACO_MS      5
#define PERIODO_BATERIA_MS   200

static bateria_t bateria;
static telemetria_amostra_t amostra;
static maq_t maq;
static botao_t botao;

/* A tel continua saindo durante o autoteste dos motores. */
static void esperar(uint32_t ms)
{
    uint32_t fim = millis() + ms;
    while ((int32_t)(millis() - fim) < 0) {
        telemetria_passo(millis(), &amostra);
        delay(PERIODO_LACO_MS);
    }
}

static void bipe(int vezes)
{
    for (int i = 0; i < vezes; i++) {
        hal_buzzer_definir(true);
        hal_led_definir(true);
        delay(100);
        hal_buzzer_definir(false);
        hal_led_definir(false);
        if (i + 1 < vezes) {
            delay(100);
        }
    }
}

static void mostrar_memoria(const char *quando)
{
    Serial.printf("# memoria %s: %s=%s %s=%s\n", quando, MEM_CHAVE_MAPA,
                  hal_nvs_existe(MEM_CHAVE_MAPA) ? "sim" : "nao", MEM_CHAVE_TIPOS,
                  hal_nvs_existe(MEM_CHAVE_TIPOS) ? "sim" : "nao");
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
    maq_evento(&maq, resultado.aprovado ? MAQ_EV_HC_APROVADO : MAQ_EV_HC_REPROVADO);
    amostra.tel.estado = maq_estado_contrato(&maq);
    amostra.tel.bat_mv = bateria_mv(&bateria);
}

/* Executa o que a máquina pediu. Este firmware não anda: só as ações paradas fazem algo. */
static void executar(maq_acao_t acao)
{
    switch (acao) {
    case MAQ_ACAO_RODAR_HEALTH_CHECK:
        rodar_health_check();
        break;
    case MAQ_ACAO_APAGAR_MAPA:
        mostrar_memoria("antes do toque curto");
        mem_apagar_mapa();
        mostrar_memoria("depois do toque curto");
        Serial.println("# BOOT curto: mapa apagado, tipos resolvidos mantidos");
        bipe(1);
        break;
    case MAQ_ACAO_APAGAR_TUDO:
        mostrar_memoria("antes do toque longo");
        mem_apagar_tudo();
        mostrar_memoria("depois do toque longo");
        Serial.println("# BOOT longo: mapa e tipos resolvidos apagados");
        bipe(2);
        break;
    default:
        break;
    }
}

static maq_evento_t evento_do_gesto(botao_gesto_t gesto)
{
    switch (gesto) {
    case BOTAO_CURTO: return MAQ_EV_BOOT_CURTO;
    case BOTAO_LONGO: return MAQ_EV_BOOT_LONGO;
    default:          return MAQ_EV_BOOT_TOQUE;
    }
}

void setup()
{
    Serial.begin(115200);
    hal_motor_iniciar(); /* motores primeiro: parados e em standby */
    hal_led_iniciar();
    hal_boot_iniciar();
    hal_encoder_iniciar();
    hal_bateria_iniciar();
    hal_dip_iniciar();
    hal_nvs_iniciar();
    hal_bt_iniciar("Ratatouille");
    hal_tof_iniciar();
    bateria_iniciar(&bateria);

    telemetria_iniciar(telemetria_proximo_boot());
    maq_iniciar(&maq);
    botao_iniciar(&botao);

    amostra = {};
    amostra.tel.estado = CONTRATO_ESTADO_HEALTH_CHECK;
    amostra.tel.rumo = CONTRATO_RUMO_N;
    amostra.tel.eixo_longo = CONTRATO_EIXO_DESCONHECIDO;
    amostra.em_movimento = false;

    mostrar_memoria("ao ligar");
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

    botao_gesto_t gesto = botao_passo(&botao, hal_boot_pressionado(), agora, maq_andando(&maq));
    if (gesto != BOTAO_NADA) {
        executar(maq_evento(&maq, evento_do_gesto(gesto)));
    }

    if (telemetria_consumir_health_check()) {
        executar(maq_evento(&maq, MAQ_EV_CMD_HEALTH_CHECK));
    }
    telemetria_consumir_largada();
    telemetria_consumir_interrupcao();
    delay(PERIODO_LACO_MS);
}
