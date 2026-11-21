#include "sinal.h"

#define PISCA_RAPIDO_MS        400
#define PISCA_LENTO_MS         2000
#define PISCA_MEDIO_MS         1000
#define PISCA_MUITO_RAPIDO_MS  200
#define PISCA_OCIOSO_MS        3000
#define PISCA_OCIOSO_ACESO_MS  100
#define SUCESSO_PISCADA_MS     200
#define SUCESSO_PISCADAS       3

void sinal_iniciar(sinal_t *s)
{
    s->estado = MAQ_OCIOSO;
    s->desde_ms = 0;
    s->iniciado = false;
}

/* Aceso na primeira metade de cada período. */
static bool metade(uint32_t desde, uint32_t periodo)
{
    return (desde % periodo) < periodo / 2;
}

bool sinal_led(sinal_t *s, maq_estado_t estado, uint32_t agora_ms)
{
    if (!s->iniciado || estado != s->estado) {
        s->iniciado = true;
        s->estado = estado;
        s->desde_ms = agora_ms;
    }
    uint32_t desde = agora_ms - s->desde_ms; /* aguenta a volta do relógio de 32 bits */

    switch (estado) {
    case MAQ_OCIOSO:
        return (desde % PISCA_OCIOSO_MS) < PISCA_OCIOSO_ACESO_MS; /* batida breve: ligado, esperando o comando */
    case MAQ_HEALTH_CHECK:
        return metade(desde, PISCA_RAPIDO_MS);
    case MAQ_AGUARDANDO:
        return true;
    case MAQ_REPROVADO:
        return metade(desde, PISCA_LENTO_MS);
    case MAQ_RUNNING:
        return metade(desde, PISCA_MEDIO_MS);
    case MAQ_SUCCESS:
        if (desde < SUCESSO_PISCADAS * SUCESSO_PISCADA_MS) {
            return metade(desde, SUCESSO_PISCADA_MS);
        }
        return true;
    case MAQ_FAILED:
        return metade(desde, PISCA_MUITO_RAPIDO_MS);
    }
    return false;
}
