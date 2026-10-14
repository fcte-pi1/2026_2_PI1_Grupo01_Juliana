#ifndef TELEMETRIA_H
#define TELEMETRIA_H

/*
 * Telemetria do firmware — FIRM-07 (#123).
 *
 * Guarda os eventos do contrato v1 (hc_item, hc_resultado, passo, falha e
 * sucesso) num buffer circular de TELEMETRIA_BUFFER_EVENTOS, para reenviá-los
 * depois de uma queda do link sem perder nem reordenar nada:
 *
 *   - os índices são absolutos (uint32_t): total conta os eventos já
 *     inseridos, qtd os que ainda estão no buffer, e o mais antigo é
 *     total - qtd;
 *   - cursor aponta o próximo evento a enviar; voltar o cursor ao mais antigo
 *     faz o reenvio;
 *   - o cabeçalho (boot, seq, t_ms) é carimbado no primeiro envio e guardado:
 *     os eventos < carimbados_ate já têm carimbo e saem sempre com ele;
 *   - buffer cheio descarta o mais antigo, e cursor e carimbados_ate avançam
 *     junto se apontavam para ele.
 *
 * Todas as operações são O(1). C puro, sem Arduino nem FreeRTOS: roda igual
 * na ESP32 e nos testes do PC.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Caminho relativo: o header do contrato também se chama telemetria.h. */
#include "../contrato/telemetria.h"

#define TELEMETRIA_BUFFER_EVENTOS 64

typedef enum {
    TELEMETRIA_EVENTO_HC_ITEM,
    TELEMETRIA_EVENTO_HC_RESULTADO,
    TELEMETRIA_EVENTO_PASSO,
    TELEMETRIA_EVENTO_FALHA,
    TELEMETRIA_EVENTO_SUCESSO,
} telemetria_tipo_evento_t;

typedef struct {
    telemetria_tipo_evento_t tipo;
    contrato_cabecalho_t cab; /* preenchido no primeiro envio */
    union {
        contrato_hc_item_t hc_item;
        contrato_hc_resultado_t hc_resultado;
        contrato_passo_t passo;
        contrato_falha_t falha;
        contrato_sucesso_t sucesso;
    } m;
} telemetria_evento_t;

/* 64 x 32 B = 2 KB de buffer na ESP32. */
_Static_assert(sizeof(telemetria_evento_t) <= 32, "telemetria_evento_t passou de 32 bytes");

typedef struct {
    telemetria_evento_t eventos[TELEMETRIA_BUFFER_EVENTOS];
    uint32_t total;          /* eventos já inseridos */
    uint32_t qtd;            /* eventos no buffer, até TELEMETRIA_BUFFER_EVENTOS */
    uint32_t cursor;         /* próximo evento a enviar */
    uint32_t carimbados_ate; /* eventos < carimbados_ate já têm cabeçalho */
} telemetria_buffer_t;

void telemetria_buffer_iniciar(telemetria_buffer_t *b);

/* Acrescenta o evento (o cabeçalho é ignorado). Cheio, descarta o mais antigo. */
void telemetria_buffer_inserir(telemetria_buffer_t *b, const telemetria_evento_t *e);

/* true se há evento no cursor esperando envio. */
bool telemetria_buffer_pendente(const telemetria_buffer_t *b);

/*
 * Serializa em buf o evento do cursor, sem avançar o cursor. Se o evento ainda
 * não tem carimbo, recebe boot, *seq (que é incrementado) e agora_ms, e o
 * carimbo fica guardado. Devolve os bytes escritos ou -1 (nada pendente ou
 * linha que não coube).
 */
int telemetria_buffer_serializar(telemetria_buffer_t *b, char *buf, size_t tam, uint32_t boot,
                                 uint32_t *seq, uint32_t agora_ms);

/* Avança o cursor depois de um envio completo. */
void telemetria_buffer_avancar(telemetria_buffer_t *b);

/* Volta o cursor ao evento mais antigo do buffer, para o reenvio. */
void telemetria_buffer_rebobinar(telemetria_buffer_t *b);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRIA_H */
