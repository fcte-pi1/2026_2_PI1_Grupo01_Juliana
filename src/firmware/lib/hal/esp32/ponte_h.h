#ifndef PONTE_H_H
#define PONTE_H_H

/*
 * Potência da HAL (-1000 a +1000) para os sinais da TB6612 — FIRM-04 (#130).
 * Separado do driver para dar para testar no PC.
 *
 * TB6612, por motor:
 *   IN1 = H, IN2 = L -> frente
 *   IN1 = L, IN2 = H -> ré
 *   IN1 = L, IN2 = L -> parado (roda livre)
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 20 kHz para não chiar; a TB6612 aceita até 100 kHz. */
#define PONTE_H_FREQUENCIA_HZ   20000
#define PONTE_H_RESOLUCAO_BITS  10
#define PONTE_H_DUTY_MAX        ((1u << PONTE_H_RESOLUCAO_BITS) - 1u)

typedef struct {
    uint16_t duty; /* 0 a PONTE_H_DUTY_MAX */
    bool in1;
    bool in2;
} ponte_h_saida_t;

/* Satura fora de ±1000. invertido troca o sentido (motor espelhado ou fios trocados). */
ponte_h_saida_t ponte_h_saida(int16_t potencia, bool invertido);

#ifdef __cplusplus
}
#endif

#endif /* PONTE_H_H */
