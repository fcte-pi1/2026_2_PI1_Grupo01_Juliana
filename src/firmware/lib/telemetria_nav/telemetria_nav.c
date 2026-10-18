#include "telemetria_nav.h"

/* Bit da parede em cada direção absoluta, na ordem de nav_direcao_t (N, L, S, O). */
static const uint8_t PAREDE_DA_DIRECAO[4] = {
    CONTRATO_PAREDE_N,
    CONTRATO_PAREDE_L,
    CONTRATO_PAREDE_S,
    CONTRATO_PAREDE_O,
};

contrato_rumo_t telemetria_rumo_do_nav(nav_direcao_t direcao)
{
    switch (direcao) {
    case NAV_LESTE:
        return CONTRATO_RUMO_L;
    case NAV_SUL:
        return CONTRATO_RUMO_S;
    case NAV_OESTE:
        return CONTRATO_RUMO_O;
    default:
        return CONTRATO_RUMO_N;
    }
}

uint8_t telemetria_paredes_do_nav(nav_direcao_t direcao, nav_leitura_t leitura)
{
    /* Girar à direita soma 1 na ordem N, L, S, O; à esquerda, soma 3. */
    uint8_t paredes = 0;
    if (leitura.frente) {
        paredes |= PAREDE_DA_DIRECAO[direcao % 4];
    }
    if (leitura.direita) {
        paredes |= PAREDE_DA_DIRECAO[(direcao + 1) % 4];
    }
    if (leitura.esquerda) {
        paredes |= PAREDE_DA_DIRECAO[(direcao + 3) % 4];
    }
    return paredes;
}
