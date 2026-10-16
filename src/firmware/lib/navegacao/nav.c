#include "nav.h"

#include <string.h>

/* Lado curto de todos os tipos: 4 células. */
#define NAV_LADO_CURTO 4

static const uint8_t PAREDE_DA_DIRECAO[4] = {
    NAV_PAREDE_N, NAV_PAREDE_L, NAV_PAREDE_S, NAV_PAREDE_O
};
static const int8_t PASSO_X[4] = {0, 1, 0, -1};
static const int8_t PASSO_Y[4] = {1, 0, -1, 0};

static nav_direcao_t girar_horario(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 1) % 4);
}

static nav_direcao_t girar_anti_horario(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 3) % 4);
}

static nav_direcao_t oposta(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 2) % 4);
}

static int lado_longo(nav_tipo_t tipo)
{
    switch (tipo) {
    case NAV_TIPO_8X4:
        return 8;
    case NAV_TIPO_12X4:
        return 12;
    default:
        return NAV_LADO_CURTO;
    }
}

/*
 * Região válida (D6). Enquanto a orientação não é resolvida, vale a união das
 * duas: um "L" com um braço em x e outro em y.
 */
static bool na_regiao(const nav_estado_t *estado, int x, int y)
{
    if (x < 0 || y < 0) {
        return false;
    }
    int longo = lado_longo(estado->tipo);
    bool longo_x = x < longo && y < NAV_LADO_CURTO;
    bool longo_y = x < NAV_LADO_CURTO && y < longo;
    switch (estado->orientacao) {
    case NAV_ORIENTACAO_LONGO_X:
        return longo_x;
    case NAV_ORIENTACAO_LONGO_Y:
        return longo_y;
    default:
        return longo_x || longo_y;
    }
}

/* Grava a parede na célula e no lado oposto da vizinha. Nunca apaga (D2). */
static void gravar_parede(nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    uint8_t parede = PAREDE_DA_DIRECAO[direcao];
    if (estado->mapa[x][y].paredes & parede) {
        return;
    }
    estado->mapa[x][y].paredes |= parede;
    estado->mapa_alterado = true;

    int vx = x + PASSO_X[direcao];
    int vy = y + PASSO_Y[direcao];
    if (vx >= 0 && vy >= 0 && vx < NAV_MAX_CELULAS && vy < NAV_MAX_CELULAS) {
        estado->mapa[vx][vy].paredes |= PAREDE_DA_DIRECAO[oposta(direcao)];
    }
}

/*
 * Na coluna x = 0, a primeira abertura lateral diz para que lado do robô fica
 * o x positivo (D5). As laterais são vistas como na largada: com rumo sul, a
 * direita física é o lado que era a esquerda.
 */
static void fixar_lado(nav_estado_t *estado, nav_leitura_t leitura)
{
    if (estado->lado != NAV_LADO_INDEFINIDO || estado->x != 0) {
        return;
    }
    bool rumo_norte = estado->direcao == NAV_NORTE;
    if (!rumo_norte && estado->direcao != NAV_SUL) {
        return;
    }
    bool aberta_direita_da_largada = rumo_norte ? !leitura.direita : !leitura.esquerda;
    bool aberta_esquerda_da_largada = rumo_norte ? !leitura.esquerda : !leitura.direita;
    if (aberta_direita_da_largada) {
        estado->lado = NAV_LADO_DIREITA;
    } else if (aberta_esquerda_da_largada) {
        estado->lado = NAV_LADO_ESQUERDA;
    }
}

/* No 8x4 e no 12x4, pisar fora do quadrado 4x4 da largada resolve a orientação. */
static void resolver_orientacao(nav_estado_t *estado)
{
    if (estado->tipo == NAV_TIPO_4X4 || estado->orientacao != NAV_ORIENTACAO_INDEFINIDA) {
        return;
    }
    if (estado->x >= NAV_LADO_CURTO) {
        estado->orientacao = NAV_ORIENTACAO_LONGO_X;
        estado->mapa_alterado = true;
    } else if (estado->y >= NAV_LADO_CURTO) {
        estado->orientacao = NAV_ORIENTACAO_LONGO_Y;
        estado->mapa_alterado = true;
    }
}

/*
 * Passa a leitura, relativa à frente do robô, para o referencial do labirinto.
 * Com o lado indefinido, as duas laterais estão fechadas (senão o lado teria
 * sido fixado), e tanto faz o espelho: viram as paredes leste e oeste.
 */
static void registrar_leitura(nav_estado_t *estado, nav_leitura_t leitura)
{
    fixar_lado(estado, leitura);

    nav_direcao_t direita = girar_horario(estado->direcao);
    nav_direcao_t esquerda = girar_anti_horario(estado->direcao);
    if (estado->lado == NAV_LADO_ESQUERDA) {
        direita = girar_anti_horario(estado->direcao);
        esquerda = girar_horario(estado->direcao);
    }

    int x = estado->x;
    int y = estado->y;
    if (leitura.frente) {
        gravar_parede(estado, x, y, estado->direcao);
    }
    if (leitura.direita) {
        gravar_parede(estado, x, y, direita);
    }
    if (leitura.esquerda) {
        gravar_parede(estado, x, y, esquerda);
    }

    estado->mapa[x][y].paredes |= NAV_VISITADA;
    resolver_orientacao(estado);
}

void nav_iniciar(nav_estado_t *estado, nav_tipo_t tipo)
{
    memset(estado, 0, sizeof(*estado));
    estado->x = 0;
    estado->y = 0;
    estado->direcao = NAV_NORTE;
    estado->tipo = tipo;
    estado->lado = NAV_LADO_INDEFINIDO;
    estado->orientacao = NAV_ORIENTACAO_INDEFINIDA;
    /* Do lado -x e atrás da largada só existe borda. */
    for (int i = 0; i < NAV_MAX_CELULAS; i++) {
        estado->mapa[i][0].paredes |= NAV_PAREDE_S;
        estado->mapa[0][i].paredes |= NAV_PAREDE_O;
    }
    nav_recalcular(estado);
}

bool nav_acessivel(const nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    if (!na_regiao(estado, x, y)) {
        return false;
    }
    if (estado->mapa[x][y].paredes & PAREDE_DA_DIRECAO[direcao]) {
        return false;
    }
    return na_regiao(estado, x + PASSO_X[direcao], y + PASSO_Y[direcao]);
}

uint8_t nav_distancia(const nav_estado_t *estado, int x, int y)
{
    if (x < 0 || y < 0 || x >= NAV_MAX_CELULAS || y >= NAV_MAX_CELULAS) {
        return NAV_DIST_INFINITA;
    }
    return estado->mapa[x][y].distancia;
}

nav_orientacao_t nav_orientacao(const nav_estado_t *estado)
{
    return estado->orientacao;
}

void nav_recalcular(nav_estado_t *estado)
{
    /* Esqueleto: a varredura de Harrison entra no próximo passo do FIRM-01. */
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            estado->mapa[x][y].distancia = NAV_DIST_INFINITA;
        }
    }
    estado->mapa_alterado = false;
}

nav_acao_t nav_passo(nav_estado_t *estado, nav_leitura_t leitura)
{
    registrar_leitura(estado, leitura);
    /* A decisão do movimento entra no próximo passo do FIRM-01 (#117). */
    return NAV_ACAO_SEM_PASSO;
}

nav_direcao_t nav_direcao_apos(nav_direcao_t atual, nav_acao_t acao)
{
    switch (acao) {
    case NAV_ACAO_DIREITA:
        return (nav_direcao_t)((atual + 1) % 4);
    case NAV_ACAO_ESQUERDA:
        return (nav_direcao_t)((atual + 3) % 4);
    case NAV_ACAO_MEIA_VOLTA:
        return (nav_direcao_t)((atual + 2) % 4);
    default:
        return atual;
    }
}
