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

/*
 * Máscara de paredes de uma célula (ordem N/S/L/O, a confirmar com o ARQ-01).
 * É derivada dos votos de nav_parede_t e é o que o flood fill usa.
 */
#define NAV_PAREDE_N 0x01
#define NAV_PAREDE_S 0x02
#define NAV_PAREDE_L 0x04
#define NAV_PAREDE_O 0x08
/* Bit extra na mesma máscara: o robô já esteve na célula. */
#define NAV_VISITADA 0x10

/*
 * Votos de uma parede (D11): ela existe no mapa se for borda, ou se não foi
 * provada aberta e tem mais votos de parede que de aberto. Empate e ausência
 * de votos contam como aberto (Adachi). O byte "trocas" guarda nos bits
 * baixos quantas vezes as leituras mudaram a parede no mapa (D13).
 */
#define NAV_TROCAS_MAX 4
#define NAV_TROCAS_MASCARA 0x0F
#define NAV_BORDA 0x40           /* sempre parede */
#define NAV_PROVADA_ABERTA 0x80  /* o robô passou por ela (D12) */

typedef struct {
    uint8_t votos_parede;
    uint8_t votos_aberto;
    uint8_t trocas;  /* contador de trocas, NAV_BORDA e NAV_PROVADA_ABERTA */
} nav_parede_t;

/*
 * Perdões por corrida (D14): sem caminho, as paredes com vantagem de no
 * máximo 1 voto são esquecidas e o flood fill é refeito.
 */
#define NAV_PERDOES_MAX 8

/* Conferências da frente por corrida (D15): na seguinte, SEM_PASSO. */
#define NAV_BLOQUEIOS_MAX 64

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
    NAV_ACAO_SUCESSO,   /* chegou a uma célula de objetivo */
    NAV_ACAO_SEM_PASSO  /* nenhum caminho possível (mapa inconsistente) */
} nav_acao_t;

typedef struct {
    uint8_t paredes;   /* NAV_PAREDE_* e NAV_VISITADA */
    uint8_t distancia; /* passos até o objetivo mais perto, ou NAV_DIST_INFINITA */
} nav_celula_t;

/*
 * Tudo o que a navegação sabe, sem ponteiros. As coordenadas são do
 * referencial do labirinto: x cresce para o lado fixado em "lado".
 *
 * A struct é plana: pode ser copiada com memcpy ou gravada e lida byte a
 * byte (na NVS, pelo FIRM-08) para retomar a tentativa de onde parou.
 */
typedef struct {
    int8_t x;
    int8_t y;
    nav_direcao_t direcao;
    nav_celula_t mapa[NAV_MAX_CELULAS][NAV_MAX_CELULAS]; /* [x][y] */
    /* Só a parede norte [0] e a leste [1]: a sul e a oeste são as da vizinha. */
    nav_parede_t paredes[NAV_MAX_CELULAS][NAV_MAX_CELULAS][2];
    nav_tipo_t tipo;
    nav_lado_t lado;
    nav_orientacao_t orientacao;
    bool mapa_alterado;         /* parede mudou ou orientação resolvida: recalcular */
    bool movimento_pendente;    /* devolveu um movimento: a próxima chamada é depois dele */
    uint8_t perdoes;            /* perdões já usados, até NAV_PERDOES_MAX */
    uint8_t bloqueios;          /* chamadas da nav_bloqueado, até NAV_BLOQUEIOS_MAX */
    bool terminou;              /* já devolveu SUCESSO ou SEM_PASSO */
    nav_acao_t acao_terminal;
} nav_estado_t;

/* Largada: célula (0, 0), virado para o norte, num labirinto do tipo dado. */
void nav_iniciar(nav_estado_t *estado, nav_tipo_t tipo);

/* Chamada uma vez por célula: registra a leitura e devolve o próximo movimento. */
nav_acao_t nav_passo(nav_estado_t *estado, nav_leitura_t leitura);

/*
 * Conferência da frente (D15): o app, já virado para o rumo da última ação,
 * viu parede à frente antes de andar. Chamar só depois de uma ação de
 * movimento devolvida pela nav_passo ou pela própria nav_bloqueado. A
 * navegação volta a pose para a célula de origem (mantendo o rumo novo),
 * soma um voto de parede, escolhe outro movimento e devolve a ação relativa
 * ao rumo atual. Com o robô parado, o app pode chamá-la de novo. Sem
 * movimento pendente, devolve SEM_PASSO sem mudar o estado.
 */
nav_acao_t nav_bloqueado(nav_estado_t *estado);

/*
 * O robô pode passar de (x, y) para a vizinha na direção dada: não há parede
 * gravada e a vizinha está na região válida do tipo e da orientação.
 */
bool nav_acessivel(const nav_estado_t *estado, int x, int y, nav_direcao_t direcao);

/* Distância calculada da célula, ou NAV_DIST_INFINITA fora da matriz ou da região. */
uint8_t nav_distancia(const nav_estado_t *estado, int x, int y);

nav_orientacao_t nav_orientacao(const nav_estado_t *estado);

/* Refaz o flood fill do mapa inteiro e desliga a flag de mapa alterado. */
void nav_recalcular(nav_estado_t *estado);

/* Direção em que o robô fica depois de executar a ação. */
nav_direcao_t nav_direcao_apos(nav_direcao_t atual, nav_acao_t acao);

#ifdef __cplusplus
}
#endif

#endif /* NAV_H */
