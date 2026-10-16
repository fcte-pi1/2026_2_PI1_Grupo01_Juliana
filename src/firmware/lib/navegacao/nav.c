#include "nav.h"

#include <string.h>

void nav_iniciar(nav_estado_t *estado, nav_tipo_t tipo)
{
    memset(estado, 0, sizeof(*estado));
    estado->x = 0;
    estado->y = 0;
    estado->direcao = NAV_NORTE;
    estado->tipo = tipo;
    estado->lado = NAV_LADO_INDEFINIDO;
    estado->orientacao = NAV_ORIENTACAO_INDEFINIDA;
    nav_recalcular(estado);
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
    /* FIRM-01 (#117): flood fill. Até lá, o esqueleto não sai do lugar. */
    (void)estado;
    (void)leitura;
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
