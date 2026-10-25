#ifndef TELEMETRIA_NAV_H
#define TELEMETRIA_NAV_H

/*
 * Conversões da navegação para o contrato — FIRM-07 (#123).
 *
 * A navegação conta as direções em N/L/S/O (nav_direcao_t) e vê as paredes
 * relativas à frente do robô; o contrato usa N/S/L/O e paredes absolutas
 * (N = +y, S = -y, L = +x, O = -x). Ficam aqui, e não em lib/navegacao, para
 * a navegação não depender do contrato, e numa lib própria para não puxar a
 * HAL de lib/telemetria. C puro.
 */

#include <stdbool.h>
#include <stdint.h>

#include "nav.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "telemetria.h" /* contrato v1, em lib/contrato */

contrato_rumo_t telemetria_rumo_do_nav(nav_direcao_t direcao);

/*
 * Máscara absoluta (soma de CONTRATO_PAREDE_*) das paredes vistas virado para
 * direcao. Com o lado NAV_LADO_ESQUERDA o x positivo fica à esquerda do robô,
 * então as laterais trocam; o lado indefinido vale como direita. A parede de
 * trás não é lida e fica fora da máscara.
 */
uint8_t telemetria_paredes_do_nav(nav_direcao_t direcao, nav_lado_t lado, nav_leitura_t leitura);

/*
 * Eventos de uma decisão, na ordem em que saem. O passo da célula decidida vem
 * antes de sucesso ou falha: o backend fecha a tentativa no primeiro dos dois,
 * e um passo que chegasse depois seria descartado (a meta ficaria fora do trajeto).
 */
typedef enum {
    TELEMETRIA_DECISAO_PASSO,
    TELEMETRIA_DECISAO_SUCESSO,
    TELEMETRIA_DECISAO_SEM_SAIDA, /* o contrato não tem "sem caminho": sai como stuck */
} telemetria_decisao_tipo_t;

typedef struct {
    telemetria_decisao_tipo_t tipo;
    uint8_t x, y;
    uint8_t paredes; /* só no passo */
} telemetria_decisao_evento_t;

typedef struct {
    bool mudou_celula;   /* a célula lida é diferente da do último passo publicado */
    int8_t x, y;         /* célula lida, antes de o passo levar a navegação à próxima */
    uint8_t paredes;     /* máscara absoluta da célula lida */
    bool terminou_agora; /* a corrida acabou nesta decisão */
    nav_acao_t acao;
    int8_t nav_x, nav_y; /* pose depois da decisão */
} telemetria_decisao_t;

#define TELEMETRIA_DECISAO_EVENTOS_MAX 2

/* Preenche out na ordem de saída e devolve quantos eventos são. */
int telemetria_eventos_da_decisao(const telemetria_decisao_t *d,
                                  telemetria_decisao_evento_t out[TELEMETRIA_DECISAO_EVENTOS_MAX]);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRIA_NAV_H */
