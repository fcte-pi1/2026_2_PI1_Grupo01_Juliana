#ifndef BOTAO_H
#define BOTAO_H

/*
 * Gestos do botão BOOT: toque durante a corrida, toque curto e toque longo, parado.
 *
 * Recebe o estado do botão e o relógio a cada chamada, sem ler HAL: roda igual na ESP32 e nos
 * testes. O contato passa por um debounce de BOTAO_DEBOUNCE_MS.
 *
 *   - em corrida, o toque sai no instante em que o botão é apertado;
 *   - parado, o toque longo sai quando o botão completa BOTAO_LONGO_MS, ainda apertado, para o
 *     bipe sair na hora; soltar depois não gera mais nada;
 *   - parado, soltar antes de BOTAO_LONGO_MS é um toque curto.
 *
 * C puro, sem HAL.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOTAO_NADA,
    BOTAO_TOQUE, /* apertou com o robô em corrida */
    BOTAO_CURTO, /* soltou antes do toque longo, parado */
    BOTAO_LONGO  /* completou o toque longo, parado */
} botao_gesto_t;

typedef struct {
    bool bruto;             /* última leitura */
    bool estavel;           /* leitura depois do debounce: true = apertado */
    uint32_t mudou_em_ms;   /* quando a leitura bruta mudou pela última vez */
    uint32_t apertou_em_ms; /* quando o aperto ficou estável */
    bool consumido;         /* o aperto atual já gerou o toque ou o toque longo */
} botao_t;

void botao_iniciar(botao_t *b);

/* Uma volta: pressionado é o nível do botão agora; em_corrida diz o que o aperto significa. */
botao_gesto_t botao_passo(botao_t *b, bool pressionado, uint32_t agora_ms, bool em_corrida);

#ifdef __cplusplus
}
#endif

#endif /* BOTAO_H */
