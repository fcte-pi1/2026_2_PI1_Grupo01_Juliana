#ifndef EXECUCAO_H
#define EXECUCAO_H

/*
 * Executa a ação que a navegação decidiu, com as primitivas de movimento:
 *
 *   FRENTE      avança uma célula
 *   DIREITA     recua, faz um arco de 90° para a direita e avança até a célula seguinte
 *   ESQUERDA    recua, faz um arco de 90° para a esquerda e avança até a célula seguinte
 *   MEIA_VOLTA  gira 20°, dá ré para levar o eixo ao lado, completa os 180° e avança até a célula
 *
 * O eixo das rodas fica atrás do centro do chassi (config/robo.h, docs/decisoes/024): girar no lugar
 * desloca o robô. As etapas são calculadas para o robô terminar com o chassi centrado na célula nova;
 * o que sobra de desvio lateral, o avanço corrige pelas paredes.
 *
 * O robô para em cada célula antes da próxima decisão (anda e para), e as paredes são lidas parado: com os
 * laterais a 90°, o sensor olha a própria célula. A tarefa chama exec_atualizar() a cada MOV_PERIODO_MS,
 * junto de mov_atualizar(), e decide de novo quando o estado voltar a EXEC_OCIOSO.
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

/* Só as ações que movem o robô. */
bool exec_eh_movimento(nav_acao_t acao);

#ifdef __cplusplus
}
#endif

#endif /* EXECUCAO_H */
