#ifndef HAL_SIM_H
#define HAL_SIM_H

/*
 * Extras que só existem na HAL simulada (ligados com -D HAL_SIMULADA).
 * Os sensores e motores simulados ficam em lib/simulacao (sim_mundo.h).
 */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Para onde vão os bytes "enviados por Bluetooth". Na ESP32 com a HAL
 * simulada, o app manda para a serial USB; nos testes, para um buffer.
 * Sem destino definido, os bytes são descartados.
 */
typedef void (*hal_bt_sim_saida_t)(const char *dados, size_t tamanho);
void hal_bt_sim_definir_saida(hal_bt_sim_saida_t saida);

/* Simula a chegada de bytes vindos da ponte (ex.: "{\"cmd\":\"interromper\"}\n"). */
void hal_bt_sim_injetar(const char *dados);

/*
 * Espelho do LED de status. O LED simulado só existe na memória; na ESP32 com
 * a HAL simulada, o app registra aqui uma função que acende o LED da placa.
 */
typedef void (*hal_led_sim_espelho_t)(bool aceso);
void hal_led_sim_definir_espelho(hal_led_sim_espelho_t espelho);

/* Simula a gravação de um firmware novo: apaga tudo da NVS. */
void hal_nvs_sim_apagar_tudo(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SIM_H */
