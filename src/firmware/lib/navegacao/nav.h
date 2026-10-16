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
 * O algoritmo (FIRM-01, #117) é o da seção "Algoritmo de navegação" do
 * docs/4.4: flood fill com a hipótese de Adachi, tipo do labirinto vindo do
 * DIP switch e orientação resolvida durante a corrida.
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
/* Bit extra na mesma máscara: o robô já esteve na célula. */
#define NAV_VISITADA 0x10

/* Distância de uma célula que não alcança nenhum objetivo. */
#define NAV_DIST_INFINITA 255

typedef enum {
    NAV_NORTE = 0,
    NAV_LESTE,
    NAV_SUL,
    NAV_OESTE
} nav_direcao_t;

/* Tipo do labirinto, lido do DIP switch. */
typedef enum {
    NAV_TIPO_4X4 = 0,
    NAV_TIPO_8X4,
    NAV_TIPO_12X4
} nav_tipo_t;

/* No 8x4 e no 12x4: para onde vai o lado longo. No 4x4 fica indefinida. */
typedef enum {
    NAV_ORIENTACAO_INDEFINIDA = 0,
    NAV_ORIENTACAO_LONGO_X,
    NAV_ORIENTACAO_LONGO_Y
} nav_orientacao_t;

/* Para que lado do robô fica o x positivo, fixado na primeira abertura lateral. */
typedef enum {
    NAV_LADO_INDEFINIDO = 0,
    NAV_LADO_DIREITA,
    NAV_LADO_ESQUERDA
} nav_lado_t;

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
    uint8_t paredes;   /* NAV_PAREDE_* e NAV_VISITADA */
    uint8_t distancia; /* passos até o objetivo mais perto, ou NAV_DIST_INFINITA */
} nav_celula_t;

/*
 * Tudo o que a navegação sabe, sem ponteiros. As coordenadas são do
 * referencial do labirinto: x cresce para o lado fixado em "lado".
 */
typedef struct {
    int8_t x;
    int8_t y;
    nav_direcao_t direcao;
    nav_celula_t mapa[NAV_MAX_CELULAS][NAV_MAX_CELULAS]; /* [x][y] */
    nav_tipo_t tipo;
    nav_lado_t lado;
    nav_orientacao_t orientacao;
    bool mapa_alterado;         /* parede nova ou orientação resolvida: recalcular */
    bool terminou;              /* já devolveu SUCESSO ou SEM_PASSO */
    nav_acao_t acao_terminal;
} nav_estado_t;

/* Largada: célula (0, 0), virado para o norte, num labirinto do tipo dado. */
void nav_iniciar(nav_estado_t *estado, nav_tipo_t tipo);

/* Chamada uma vez por célula: registra a leitura e devolve o próximo movimento. */
nav_acao_t nav_passo(nav_estado_t *estado, nav_leitura_t leitura);

/*
 * O robô pode passar de (x, y) para a vizinha na direção dada: não há parede
 * gravada e a vizinha está na região válida do tipo e da orientação.
 */
bool nav_acessivel(const nav_estado_t *estado, int x, int y, nav_direcao_t direcao);

/* Distância calculada da célula, ou NAV_DIST_INFINITA fora da matriz. */
uint8_t nav_distancia(const nav_estado_t *estado, int x, int y);

nav_orientacao_t nav_orientacao(const nav_estado_t *estado);

/* Refaz o flood fill do mapa inteiro. */
void nav_recalcular(nav_estado_t *estado);

/* Direção em que o robô fica depois de executar a ação. */
nav_direcao_t nav_direcao_apos(nav_direcao_t atual, nav_acao_t acao);

#ifdef __cplusplus
}
#endif

#endif /* NAV_H */
