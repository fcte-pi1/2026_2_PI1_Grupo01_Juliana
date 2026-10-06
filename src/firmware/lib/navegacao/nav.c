#include "nav.h"

void nav_iniciar(nav_estado_t *estado)
{
    estado->x = 0;
    estado->y = 0;
    estado->direcao = NAV_NORTE;
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
