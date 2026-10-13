#include "bateria.h"

#include <stddef.h>

typedef struct {
    uint16_t mv;
    uint8_t pct;
} ponto_curva_t;

static const ponto_curva_t CURVA_LIPO[] = BATERIA_CURVA_LIPO;
static const ponto_curva_t CURVA_AA[] = BATERIA_CURVA_AA;

#define TAMANHO(v) (sizeof(v) / sizeof((v)[0]))

void bateria_iniciar(bateria_t *b)
{
    *b = (bateria_t){0};
    b->fonte = BATERIA_FONTE_LIPO;
}

void bateria_amostrar(bateria_t *b, uint16_t adc_mv)
{
    if (b->quantidade == BATERIA_MEDIA_AMOSTRAS) {
        b->soma -= b->amostras[b->proxima];
    } else {
        b->quantidade++;
    }
    b->amostras[b->proxima] = adc_mv;
    b->soma += adc_mv;
    b->proxima = (uint8_t)((b->proxima + 1) % BATERIA_MEDIA_AMOSTRAS);

    if (!b->fonte_fixada && bateria_pronta(b)) {
        b->fonte = bateria_detectar_fonte(bateria_mv(b));
        b->fonte_fixada = true;
    }
}

bool bateria_pronta(const bateria_t *b) { return b->quantidade == BATERIA_MEDIA_AMOSTRAS; }

uint16_t bateria_ponte_mv(const bateria_t *b)
{
    if (b->quantidade == 0) {
        return 0;
    }
    float media_adc = (float)b->soma / (float)b->quantidade;
    return (uint16_t)(media_adc * BATERIA_DIVISOR + 0.5f);
}

uint16_t bateria_mv(const bateria_t *b)
{
    uint16_t ponte = bateria_ponte_mv(b);
    return ponte == 0 ? 0 : (uint16_t)(ponte + BATERIA_QUEDA_DIODO_MV);
}

bateria_fonte_t bateria_fonte(const bateria_t *b)
{
    return b->fonte_fixada ? b->fonte : bateria_detectar_fonte(bateria_mv(b));
}

uint8_t bateria_percentual(const bateria_t *b)
{
    return bateria_percentual_de(bateria_fonte(b), bateria_mv(b));
}

bateria_nivel_t bateria_nivel(const bateria_t *b)
{
    return bateria_nivel_de(bateria_fonte(b), bateria_mv(b));
}

bateria_fonte_t bateria_detectar_fonte(uint16_t fonte_mv)
{
    return fonte_mv >= BATERIA_LIMIAR_FONTE_MV ? BATERIA_FONTE_LIPO : BATERIA_FONTE_AA;
}

/* Interpola na curva {mV, %}, ordenada da maior para a menor tensão. */
static uint8_t interpolar(const ponto_curva_t *curva, size_t n, uint16_t mv)
{
    if (mv >= curva[0].mv) {
        return curva[0].pct;
    }
    for (size_t i = 1; i < n; i++) {
        if (mv >= curva[i].mv) {
            const ponto_curva_t *alto = &curva[i - 1];
            const ponto_curva_t *baixo = &curva[i];
            uint32_t faixa_mv = (uint32_t)(alto->mv - baixo->mv);
            uint32_t acima_mv = (uint32_t)(mv - baixo->mv);
            uint32_t faixa_pct = (uint32_t)(alto->pct - baixo->pct);
            return (uint8_t)(baixo->pct + (acima_mv * faixa_pct + faixa_mv / 2) / faixa_mv);
        }
    }
    return curva[n - 1].pct;
}

uint8_t bateria_percentual_de(bateria_fonte_t fonte, uint16_t fonte_mv)
{
    if (fonte == BATERIA_FONTE_AA) {
        return interpolar(CURVA_AA, TAMANHO(CURVA_AA), fonte_mv);
    }
    return interpolar(CURVA_LIPO, TAMANHO(CURVA_LIPO), fonte_mv);
}

bateria_nivel_t bateria_nivel_de(bateria_fonte_t fonte, uint16_t fonte_mv)
{
    uint16_t corte = fonte == BATERIA_FONTE_AA ? BATERIA_AA_CORTE_MV : BATERIA_CORTE_MV;
    uint16_t alerta = fonte == BATERIA_FONTE_AA ? BATERIA_AA_ALERTA_MV : BATERIA_ALERTA_MV;
    if (fonte_mv < corte) {
        return BATERIA_BAIXA;
    }
    if (fonte_mv <= alerta) {
        return BATERIA_ALERTA;
    }
    return BATERIA_NORMAL;
}
