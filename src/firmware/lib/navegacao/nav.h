#ifndef NAV_H
#define NAV_H

/*
 * Navegação: decide o próximo movimento a partir das paredes da célula atual.
 *
 * REGRA DESTA PASTA: C puro. Nada de Arduino, FreeRTOS ou HAL aqui dentro.
 * As paredes chegam como parâmetro e a decisão sai como retorno; quem lê os
 * sensores e move o robô é o app. Assim o mesmo código roda na ESP32, nos
 * testes e no simulador do PC (FIRM-02, #114).
 *
 * O algoritmo (flood fill com Adachi, tipo do labirinto e prova de borda) é o
 * FIRM-01 (#117). Esta interface pode mudar quando ele for implementado.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NAV_MAX_CELULAS 12

/* Máscara de paredes de uma célula (ordem N/S/L/O, a confirmar com o ARQ-01). */
#define NAV_PAREDE_N 0x01
#define NAV_PAREDE_S 0x02
#define NAV_PAREDE_L 0x04
#define NAV_PAREDE_O 0x08

typedef enum {
    NAV_NORTE = 0,
    NAV_LESTE,
    NAV_SUL,
    NAV_OESTE
} nav_direcao_t;

/* O que os sensores viram na célula atual, relativo à frente do robô. */
typedef struct {
    bool frente;
    bool esquerda;
    bool direita;
} nav_leitura_t;

/* Próximo movimento. Os giros já incluem avançar uma célula depois. */
typedef enum {
    NAV_ACAO_FRENTE = 0,
    NAV_ACAO_DIREITA,
    NAV_ACAO_ESQUERDA,
    NAV_ACAO_MEIA_VOLTA,
    NAV_ACAO_SUCESSO,   /* chegou ao objetivo com a borda provada */
    NAV_ACAO_SEM_PASSO  /* nenhum caminho possível (mapa inconsistente) */
} nav_acao_t;

typedef struct {
    int8_t x;
    int8_t y;
    nav_direcao_t direcao;
    /* FIRM-01: mapa de paredes, distâncias, extensão e objetivos candidatos. */
} nav_estado_t;

/* Largada: célula (0, 0), virado para o norte. */
void nav_iniciar(nav_estado_t *estado);

/* Chamada uma vez por célula: registra a leitura e devolve o próximo movimento. */
nav_acao_t nav_passo(nav_estado_t *estado, nav_leitura_t leitura);

/* Direção em que o robô fica depois de executar a ação. */
nav_direcao_t nav_direcao_apos(nav_direcao_t atual, nav_acao_t acao);

#ifdef __cplusplus
}
#endif

#endif /* NAV_H */
