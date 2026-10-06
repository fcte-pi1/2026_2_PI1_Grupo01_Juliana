#ifndef HAL_TEMPO_H
#define HAL_TEMPO_H

/*
 * Relógio do firmware. Na ESP32 real é o millis(); na simulação é um relógio
 * virtual que só anda quando sim_mundo_avancar() é chamado, o que deixa os
 * testes no PC rápidos e repetíveis.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Milissegundos desde o boot. */
uint32_t hal_tempo_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_TEMPO_H */
