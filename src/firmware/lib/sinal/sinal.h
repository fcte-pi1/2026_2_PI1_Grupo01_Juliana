#ifndef SINAL_H
#define SINAL_H

/*
 * Padrão do LED para cada estado do robô, em função do tempo.
 *
 *   health-check       pisca rápido  (400 ms, 200 aceso)
 *   aguardando largar  aceso fixo
 *   reprovado          pisca lento   (2000 ms, 1000 aceso)
 *   running            pisca médio   (1000 ms, 500 aceso)
 *   success            3 piscadas (200 ms) e fica aceso
 *   failed             pisca muito rápido (200 ms, 100 aceso)
 *
 * Não lê HAL nem tempo: quem chama passa o estado e o relógio. C puro.
 */

#include <stdbool.h>
#include <stdint.h>

#include "maquina.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    maq_estado_t estado;
    uint32_t desde_ms; /* instante em que o estado atual começou */
    bool iniciado;
} sinal_t;

void sinal_iniciar(sinal_t *s);

/* O LED deve estar aceso agora? A mudança de estado reinicia o padrão. */
bool sinal_led(sinal_t *s, maq_estado_t estado, uint32_t agora_ms);

#ifdef __cplusplus
}
#endif

#endif /* SINAL_H */
