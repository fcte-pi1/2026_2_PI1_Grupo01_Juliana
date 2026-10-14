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
 * Por cima do buffer, telemetria_passo() decide o que sai em cada instante:
 * a tel a 5 Hz em movimento e a 1 Hz parado, e no máximo um evento a cada
 * TELEMETRIA_PERIODO_EVENTO_MS. O relógio entra como parâmetro. Na mesma
 * volta, lê o que chegou da ponte e reconhece o comando interromper.
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

#define TELEMETRIA_PERIODO_TEL_MOVIMENTO_MS 200 /* 5 Hz */
#define TELEMETRIA_PERIODO_TEL_PARADO_MS 1000   /* 1 Hz */
#define TELEMETRIA_PERIODO_EVENTO_MS 50         /* drena até 20 eventos/s */

/* Linha recebida da ponte, contando o "\n"; maior que isso é descartada. */
#define TELEMETRIA_LINHA_RECEBIDA 256

/* Chave da NVS com o contador de inicializações (uint32_t). */
#define TELEMETRIA_CHAVE_BOOT "boot"

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
    uint32_t descartados;    /* eventos que saíram do buffer sem nunca ter sido enviados */
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

/* O que a navegação publica a cada volta: os campos da tel e se está andando. */
typedef struct {
    contrato_tel_t tel;
    bool em_movimento;
} telemetria_amostra_t;

/* Zera o buffer, o seq, os prazos e a leitura; boot vai em todos os cabeçalhos. */
void telemetria_iniciar(uint32_t boot);

/* Guarda um evento para envio (o cabeçalho é ignorado). */
void telemetria_inserir_evento(const telemetria_evento_t *e);

/*
 * Uma volta da telemetria no instante agora_ms: confere o link (na volta dele,
 * o reenvio recomeça do mais antigo), envia a tel se o prazo venceu e no
 * máximo um evento. amostra NULL não envia tel. Escrita parcial conta como
 * queda. Devolve os ms até o próximo prazo, no máximo
 * TELEMETRIA_PERIODO_EVENTO_MS.
 */
uint32_t telemetria_passo(uint32_t agora_ms, const telemetria_amostra_t *amostra);

/* Eventos perdidos por buffer cheio antes do primeiro envio (depuração). */
uint32_t telemetria_descartados(void);

/*
 * true se chegou o comando interromper desde a última chamada; a flag é zerada
 * na mesma operação. Pode ser chamada de outro núcleo.
 */
bool telemetria_consumir_interrupcao(void);

/*
 * Lê o contador boot da NVS (chave TELEMETRIA_CHAVE_BOOT), soma 1, grava e
 * devolve o novo valor; sem a chave, o primeiro boot é 1. Se a gravação
 * falhar, devolve o valor somado mesmo assim. Chamar depois de hal_nvs_iniciar.
 */
uint32_t telemetria_proximo_boot(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRIA_H */
