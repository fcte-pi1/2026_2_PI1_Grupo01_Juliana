#ifndef ENCODER_SOMA_H
#define ENCODER_SOMA_H

/*
 * Soma dos pulsos de um encoder a partir do contador de 16 bits do PCNT.
 *
 * O PCNT zera o contador ao chegar em +limite ou -limite. Quem lê o contador com
 * frequência (o laço de controle, a 200 Hz) percebe o salto e soma o limite ao
 * acumulado, sem precisar de interrupção. Vale enquanto o contador não andar
 * mais que metade do limite entre duas leituras. Separado do driver para dar
 * para testar no PC.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t acumulado; /* múltiplos do limite já percorridos */
    int16_t ultimo;    /* última contagem lida */
} encoder_soma_t;

/* Começa do zero, com o contador do hardware em `contagem`. */
void encoder_soma_iniciar(encoder_soma_t *s, int16_t contagem);

/* Total de pulsos com sinal depois de ler `contagem`. */
int32_t encoder_soma_atualizar(encoder_soma_t *s, int16_t contagem, int32_t limite);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_SOMA_H */
