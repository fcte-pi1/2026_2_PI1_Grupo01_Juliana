#ifndef SIM_ROTEIRO_H
#define SIM_ROTEIRO_H

/*
 * Roteiro de telemetria de uma corrida simulada, no formato do contrato v1
 * (docs/4.4.1, ARQ-01 #107): um JSON por linha, com as mesmas chaves e a
 * mesma ordem que o firmware envia. Serve de entrada para o simulador de
 * robô do ARQ-07, que reproduz o roteiro para o backend como se fosse a ponte.
 *
 * Sequência gerada: health-check (8 hc_item e o hc_resultado), um "passo" a
 * cada célula, "tel" a 5 Hz em movimento (1 Hz parado) e, no fim, "sucesso"
 * ou "falha". Os tempos vêm de um modelo simples de velocidade constante e os
 * valores do health-check são fictícios.
 *
 * Os serializadores serão trocados pelos do contrato (src/firmware/contrato)
 * quando o ARQ-01 entrar na main; as linhas geradas não mudam.
 */

#include <stdbool.h>
#include <stdint.h>

#include "sim_corrida.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_ROTEIRO_TAMANHO_LINHA 256 /* limite do contrato, contando o "\n" */

/* Recebe cada linha completa, já com o "\n". */
typedef void (*sim_saida_t)(void *ctx, const char *linha);

typedef struct {
    sim_saida_t saida;
    void *saida_ctx;
    uint32_t boot;
    uint32_t seq;
    uint32_t t_ms;
    uint32_t proxima_tel_ms;
    const char *estado;     /* "health-check", "running", "success" ou "failed" */
    uint8_t x, y;
    nav_direcao_t rumo;
    char eixo_longo;        /* 'x', 'y' ou 0 (null) */
} sim_roteiro_t;

/* Começa o roteiro e já escreve o health-check aprovado. */
void sim_roteiro_iniciar(sim_roteiro_t *r, uint32_t boot, sim_tipo_t tipo, sim_saida_t saida, void *saida_ctx);

/* Observador da corrida (sim_config_t.observador, com ctx = o roteiro). */
void sim_roteiro_celula(void *roteiro, const sim_celula_t *celula);

/* Escreve o "sucesso" ou a "falha" conforme o resultado da corrida. */
void sim_roteiro_finalizar(sim_roteiro_t *r, const sim_relatorio_t *relatorio);

#ifdef __cplusplus
}
#endif

#endif /* SIM_ROTEIRO_H */
