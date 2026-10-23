#ifndef RETOMADA_H
#define RETOMADA_H

/*
 * Mapa na NVS — FIRM-08 (#137).
 *
 * Grava, a cada célula nova, o mapa, a posição, a orientação, o estado e os
 * tipos resolvidos. O registro tem versão e CRC. No boot, retomada_carregar
 * devolve esse estado para a corrida continuar da célula gravada.
 *
 * CRC errado, versão desconhecida, registro ausente ou tipo diferente do DIP
 * viram mapa vazio (nav_iniciar). A gravação cabe em RETOMADA_LIMITE_US.
 */

#include <stdbool.h>

#include "nav.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETOMADA_LIMITE_US 20000

/* Copia o estado para a NVS. Não altera *estado. false se a NVS recusar. */
bool retomada_gravar(const nav_estado_t *estado);

/*
 * true se carregou o mapa e a célula gravados. false deixa *estado como
 * nav_iniciar(tipo_dip).
 */
bool retomada_carregar(nav_estado_t *estado, nav_tipo_t tipo_dip);

typedef struct {
    int8_t x;
    int8_t y;
    bool gravou;
} retomada_sessao_t;

void retomada_sessao_iniciar(retomada_sessao_t *sessao);

/* Grava só quando a célula mudou. A primeira chamada grava a célula atual. */
bool retomada_na_celula(retomada_sessao_t *sessao, const nav_estado_t *estado);

#ifdef __cplusplus
}
#endif

#endif /* RETOMADA_H */
