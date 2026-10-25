#ifndef MEMORIA_H
#define MEMORIA_H

/*
 * Mapa e estado na NVS para retomar depois de um reset — FIRM-08 (#137).
 *
 * Duas chaves, cada uma com versão e CRC32:
 *   MEM_CHAVE_MAPA:  mapa, posição, direção, contadores da corrida e estado da execução
 *   MEM_CHAVE_TIPOS: tipo do labirinto, orientação e lado
 * Toque curto do BOOT (FIRM-09) apaga só o mapa; o longo apaga as duas.
 *
 * Gravar só com o robô já na célula (ADR-012); quem chama escolhe o momento.
 * Se o nav_estado_t mudar, suba MEM_VERSAO.
 */

#include <stdbool.h>

#include "nav.h"
#include "telemetria.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_VERSAO      1
#define MEM_CHAVE_MAPA  "mem_mapa"
#define MEM_CHAVE_TIPOS "mem_tipos"

typedef enum {
    MEM_NOVA = 0,  /* nada válido: mapa vazio */
    MEM_SO_TIPOS,  /* mapa vazio, mas com tipo, orientação e lado de antes */
    MEM_RETOMADA,  /* mapa e posição gravados: hc_resultado com inicio "retomada" */
} mem_inicio_t;

/* Grava as duas chaves. A de tipos só é regravada se mudou. */
bool mem_gravar(const nav_estado_t *nav, contrato_estado_t estado);

/*
 * Grava se a célula mudou desde a última gravada, ou se forcar (fim da
 * corrida na mesma célula). Sem mudança, não escreve e devolve true.
 * Em sucesso, *celula_x e *celula_y passam a ser a célula gravada.
 */
bool mem_gravar_celula(const nav_estado_t *nav, contrato_estado_t estado, int8_t *celula_x,
                       int8_t *celula_y, bool forcar);

/*
 * Um passo da tarefa de navegação: registra a leitura, e grava a NVS se a
 * célula mudou ou se a corrida acabou. Devolve a ação. Com a corrida já
 * terminada, não altera o estado e devolve a ação terminal.
 */
nav_acao_t mem_aplicar_leitura(nav_estado_t *nav, contrato_estado_t *estado, int8_t *celula_x,
                               int8_t *celula_y, bool *terminou, nav_leitura_t leitura);

/*
 * Carrega o que for válido para o tipo do DIP. CRC errado, versão diferente ou
 * outro tipo contam como ausentes. Sem mapa válido, *nav sai com o mapa vazio e
 * *estado como CONTRATO_ESTADO_HEALTH_CHECK.
 */
mem_inicio_t mem_carregar(nav_estado_t *nav, contrato_estado_t *estado, nav_tipo_t tipo_dip);

/* Toque curto do BOOT: apaga mapa e posição, mantém os tipos resolvidos. */
void mem_apagar_mapa(void);

/* Toque longo do BOOT: apaga as duas chaves. */
void mem_apagar_tudo(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMORIA_H */
