#include "botao.h"

#include "config/robo.h"

void botao_iniciar(botao_t *b)
{
    b->bruto = false;
    b->estavel = false;
    b->mudou_em_ms = 0;
    b->apertou_em_ms = 0;
    b->consumido = false;
}

botao_gesto_t botao_passo(botao_t *b, bool pressionado, uint32_t agora_ms, bool em_corrida)
{
    if (pressionado != b->bruto) {
        b->bruto = pressionado;
        b->mudou_em_ms = agora_ms;
        return BOTAO_NADA;
    }

    if (b->estavel != b->bruto && (uint32_t)(agora_ms - b->mudou_em_ms) >= BOTAO_DEBOUNCE_MS) {
        b->estavel = b->bruto;
        if (b->estavel) {
            b->apertou_em_ms = b->mudou_em_ms;
            b->consumido = false;
            if (em_corrida) {
                b->consumido = true;
                return BOTAO_TOQUE;
            }
        } else {
            bool curto = !b->consumido;
            b->consumido = false;
            return curto ? BOTAO_CURTO : BOTAO_NADA;
        }
    }

    /* A leitura bruta também precisa estar apertada: soltar durante o debounce não vira longo. */
    if (b->estavel && b->bruto && !b->consumido && !em_corrida &&
        (uint32_t)(agora_ms - b->apertou_em_ms) >= BOTAO_LONGO_MS) {
        b->consumido = true;
        return BOTAO_LONGO;
    }
    return BOTAO_NADA;
}
