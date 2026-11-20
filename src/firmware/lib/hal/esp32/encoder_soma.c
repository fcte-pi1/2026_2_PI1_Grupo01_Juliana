#include "encoder_soma.h"

void encoder_soma_iniciar(encoder_soma_t *s, int16_t contagem)
{
    s->acumulado = -(int32_t)contagem;
    s->ultimo = contagem;
}

int32_t encoder_soma_atualizar(encoder_soma_t *s, int16_t contagem, int32_t limite)
{
    int32_t salto = (int32_t)contagem - (int32_t)s->ultimo;
    if (salto < -limite / 2) {
        s->acumulado += limite; /* passou de +limite e o contador zerou */
    } else if (salto > limite / 2) {
        s->acumulado -= limite; /* passou de -limite e o contador zerou */
    }
    s->ultimo = contagem;
    return s->acumulado + contagem;
}
