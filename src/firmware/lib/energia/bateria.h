#ifndef BATERIA_H
#define BATERIA_H

/*
 * Leitura da bateria — FIRM-06 (#128).
 *
 * Recebe a tensão no pino do ADC (hal_bateria_ler_adc_mv) e devolve a tensão
 * da fonte, o percentual de carga e o nível (normal, alerta ou low_battery),
 * conforme a seção 6 do docs/4.2:
 *
 *   - média móvel de BATERIA_MEDIA_AMOSTRAS leituras, contra o ruído das escovas;
 *   - V_ponte = V_adc x BATERIA_DIVISOR: tensão depois do diodo SS34, que é a
 *     que chega à ponte H (é a usada no limite de PWM);
 *   - V_fonte = V_ponte + BATERIA_QUEDA_DIODO_MV: tensão da bateria, usada na
 *     telemetria, no percentual e nos limites de alerta e corte;
 *   - a fonte ativa (LiPo ou pilhas) é deduzida pela tensão quando a primeira
 *     janela da média fica cheia, e não muda mais até bateria_iniciar.
 *
 * C puro, sem HAL: roda igual na ESP32 e nos testes do PC.
 */

#include <stdbool.h>
#include <stdint.h>

#include "config/robo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BATERIA_FONTE_LIPO,
    BATERIA_FONTE_AA,
} bateria_fonte_t;

typedef enum {
    BATERIA_NORMAL,
    BATERIA_ALERTA, /* LED e buzzer (RF05 de Energia) */
    BATERIA_BAIXA,  /* abaixo do corte: falha low_battery */
} bateria_nivel_t;

typedef struct {
    uint16_t amostras[BATERIA_MEDIA_AMOSTRAS];
    uint32_t soma;
    uint8_t proxima;
    uint8_t quantidade;
    bateria_fonte_t fonte;
    bool fonte_fixada;
} bateria_t;

void bateria_iniciar(bateria_t *b);

/* Acrescenta uma leitura do ADC (mV no pino) à média móvel. */
void bateria_amostrar(bateria_t *b, uint16_t adc_mv);

/* true quando a janela da média já está cheia. Antes disso, a média usa as leituras que houver. */
bool bateria_pronta(const bateria_t *b);

/* Tensão depois do diodo, na ponte H, em mV. 0 sem nenhuma leitura. */
uint16_t bateria_ponte_mv(const bateria_t *b);

/* Tensão da fonte, em mV. 0 sem nenhuma leitura. */
uint16_t bateria_mv(const bateria_t *b);

bateria_fonte_t bateria_fonte(const bateria_t *b);
uint8_t bateria_percentual(const bateria_t *b);
bateria_nivel_t bateria_nivel(const bateria_t *b);

/* Funções puras, sobre a tensão da fonte em mV. */
bateria_fonte_t bateria_detectar_fonte(uint16_t fonte_mv);
uint8_t bateria_percentual_de(bateria_fonte_t fonte, uint16_t fonte_mv);
bateria_nivel_t bateria_nivel_de(bateria_fonte_t fonte, uint16_t fonte_mv);

#ifdef __cplusplus
}
#endif

#endif /* BATERIA_H */
