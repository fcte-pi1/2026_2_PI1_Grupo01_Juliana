#ifndef CORRIDA_H
#define CORRIDA_H

/*
 * Estado da corrida: junta a máquina de estados, a navegação e a NVS.
 *
 * A tarefa de navegação traduz botão, comandos e health-check em eventos, e
 * executa os efeitos que esta lib pede (rodar o health-check, parar os motores,
 * bipar, enviar a falha, reposicionar o robô na simulação). Não lê HAL de
 * tempo nem de pinos, só a NVS: roda igual na ESP32 e nos testes do PC.
 */

#include <stdbool.h>
#include <stdint.h>

#include "maquina.h"
#include "memoria.h"
#include "nav.h"
#include "telemetria.h"
#include "telemetria_nav.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    maq_t maq;
    nav_estado_t nav;
    contrato_estado_t estado; /* o que a NVS grava */
    nav_tipo_t tipo;
    mem_inicio_t inicio;
    nav_acao_t acao;          /* última ação da navegação */
    bool terminou;
    int8_t ultimo_x, ultimo_y;   /* célula do último passo publicado */
    int8_t gravada_x, gravada_y; /* célula gravada na NVS */
} corrida_t;

/* O que quem chamou deve fazer depois de um evento. */
typedef struct {
    bool rodar_health_check;
    bool parar_motores;
    bool reposicionar;    /* a pose da navegação mudou: simulação vai para a célula */
    int bipes;            /* 0, 1 ou 2 */
    bool tem_falha;
    contrato_falha_t falha;
} corrida_efeitos_t;

/* Carrega a NVS para o tipo do DIP e começa em health-check. */
void corrida_iniciar(corrida_t *c, nav_tipo_t tipo_dip);

/* Evento da máquina (comando, BOOT, health-check pedido). */
void corrida_evento(corrida_t *c, maq_evento_t evento, corrida_efeitos_t *efeitos);

/*
 * O health-check terminou. Se o DIP agora indica outro tipo, a navegação
 * recomeça (efeitos->reposicionar).
 */
void corrida_health_check_concluido(corrida_t *c, bool aprovado, nav_tipo_t tipo_dip,
                                    corrida_efeitos_t *efeitos);

/*
 * Uma decisão da navegação. Só age em RUNNING; fora dela devolve 0 eventos.
 * Preenche os eventos do contrato na ordem de saída e devolve quantos são;
 * *reposicionar fica true se a pose mudou.
 */
int corrida_decidir(corrida_t *c, nav_leitura_t leitura, bool *reposicionar,
                    telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX]);

/*
 * O movimento falhou (roda travada ou fora do tempo): a corrida vai a failed, grava a NVS e pede a
 * falha automática `stuck` na célula atual. Só age em RUNNING.
 */
void corrida_movimento_falhou(corrida_t *c, corrida_efeitos_t *efeitos);

contrato_estado_t corrida_estado_contrato(const corrida_t *c);

/* true só andando e sem ação terminal. */
bool corrida_em_movimento(const corrida_t *c);

#ifdef __cplusplus
}
#endif

#endif /* CORRIDA_H */
