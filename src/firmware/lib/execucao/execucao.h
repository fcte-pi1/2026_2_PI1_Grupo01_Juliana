#ifndef EXECUCAO_H
#define EXECUCAO_H

/*
 * Executa a ação que a navegação decidiu, com as primitivas de movimento:
 *
 *   FRENTE      avança uma célula
 *   DIREITA     gira -90° e avança uma célula
 *   ESQUERDA    gira +90° e avança uma célula
 *   MEIA_VOLTA  gira 180° e avança uma célula
 *
 * O robô para no centro de cada célula antes da próxima decisão (anda e para): é aí que os ToF
 * medem a parede lateral com confiança (config/robo.h). A tarefa chama exec_atualizar() a cada
 * MOV_PERIODO_MS, junto de mov_atualizar(), e decide de novo quando o estado voltar a EXEC_OCIOSO.
 *
 * Não lê HAL nem relógio: roda igual na ESP32 e nos testes do PC.
 */

#include <stdbool.h>

#include "movimento.h"
#include "nav.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EXEC_OCIOSO,    /* nada em andamento: o robô está parado no centro de uma célula */
    EXEC_GIRANDO,
    EXEC_AVANCANDO,
    EXEC_FALHOU     /* roda travada ou movimento fora do tempo; o robô está parado */
} exec_estado_t;

void exec_iniciar(void);

/* Começa a ação. false se não for uma ação de movimento ou se já houver uma em andamento. */
bool exec_comecar(nav_acao_t acao);

/* Um ciclo: avança as etapas da ação e devolve o estado depois dele. */
exec_estado_t exec_atualizar(void);

exec_estado_t exec_estado(void);

/* Cancela o que estiver em andamento e para os motores. */
void exec_parar(void);

/*
 * true uma única vez por ação, quando o avanço chega a EXEC_LEITURA_LATERAL_MM: é a hora de guardar
 * a parede lateral da célula nova (o robô acabou de entrar nela).
 */
bool exec_ler_lateral_agora(void);

/* Só as ações que movem o robô. */
bool exec_eh_movimento(nav_acao_t acao);

#ifdef __cplusplus
}
#endif

#endif /* EXECUCAO_H */
