#ifndef HAL_DIP_H
#define HAL_DIP_H

/* DIP switch de 4 vias (GPIO23, 17, 16 e 19) */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configura os pinos com pull-up interno. */
void hal_dip_iniciar(void);

/*
 * Vias ligadas, uma por bit: bit 0 = via 1, ..., bit 3 = via 4. A chave ligada
 * puxa o pino para o terra, e o driver já inverte o nível: 1 = via ligada.
 */
uint8_t hal_dip_ler(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_DIP_H */
