#include "app.h"

#include <Arduino.h>

#include "config/pinos.h"
#include "eventos.h"
#include "hal_bateria.h"
#include "hal_boot.h"
#include "hal_bt.h"
#include "hal_led.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "movimento.h"
#include "sensores.h"

#ifdef HAL_SIMULADA
#include "labirinto_demo.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"
#endif

#define TAMANHO_FILA        16
#define PILHA_TAREFA        4096
#define NUCLEO_TELEMETRIA   0  /* o núcleo 0 também roda o Bluetooth da ESP32 */
#define NUCLEO_NAVEGACAO    1

QueueHandle_t fila_eventos;

#ifdef HAL_SIMULADA
/* Com a HAL simulada, o "Bluetooth" sai pela serial USB (FIRM-07 usa o mesmo caminho). */
static void enviar_pela_serial(const char *dados, size_t tamanho)
{
    Serial.write(reinterpret_cast<const uint8_t *>(dados), tamanho);
}

/* O LED da própria placa existe mesmo sem robô, então a simulação o usa. */
static void acender_led_da_placa(bool aceso)
{
    digitalWrite(PINO_LED_STATUS, aceso ? HIGH : LOW);
}
#endif

void app_iniciar(void)
{
#ifdef HAL_SIMULADA
    sim_mundo_iniciar(LABIRINTO_DEMO);
    hal_bt_sim_definir_saida(enviar_pela_serial);
    pinMode(PINO_LED_STATUS, OUTPUT);
    hal_led_sim_definir_espelho(acender_led_da_placa);
    Serial.println("# Ratatouille: firmware com HAL SIMULADA");
#endif

    /* Motores primeiro: o robô precisa ficar parado desde o início. */
    mov_iniciar();
    hal_led_iniciar();
    hal_boot_iniciar();
    hal_bateria_iniciar();
    hal_nvs_iniciar();
    hal_bt_iniciar("Ratatouille");

    if (!hal_tof_iniciar()) {
        Serial.println("# aviso: algum ToF não respondeu");
    }
    sensores_iniciar();

    fila_eventos = xQueueCreate(TAMANHO_FILA, sizeof(evento_leitura_t));

    xTaskCreatePinnedToCore(tarefa_telemetria, "telemetria", PILHA_TAREFA, NULL, 1, NULL,
                            NUCLEO_TELEMETRIA);
    xTaskCreatePinnedToCore(tarefa_navegacao, "navegacao", PILHA_TAREFA, NULL, 2, NULL,
                            NUCLEO_NAVEGACAO);
}
