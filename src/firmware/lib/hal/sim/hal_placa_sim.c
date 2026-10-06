#ifdef HAL_SIMULADA

/* Bateria, BOOT, LED/buzzer e relógio simulados. */

#include "hal_bateria.h"
#include "hal_boot.h"
#include "hal_led.h"
#include "hal_sim.h"
#include "hal_tempo.h"
#include "sim_mundo.h"

static hal_led_sim_espelho_t espelho_led;

void hal_bateria_iniciar(void) {}
uint16_t hal_bateria_ler_adc_mv(void) { return sim_bateria_adc_mv(); }

void hal_boot_iniciar(void) {}
bool hal_boot_pressionado(void) { return sim_boot_pressionado(); }

void hal_led_iniciar(void)
{
    hal_led_definir(false);
    sim_buzzer_definir(false);
}

void hal_led_definir(bool aceso)
{
    sim_led_definir(aceso);
    if (espelho_led != NULL) {
        espelho_led(aceso);
    }
}

void hal_led_sim_definir_espelho(hal_led_sim_espelho_t espelho) { espelho_led = espelho; }

void hal_buzzer_definir(bool ligado) { sim_buzzer_definir(ligado); }

uint32_t hal_tempo_ms(void) { return sim_tempo_ms(); }

#endif /* HAL_SIMULADA */
