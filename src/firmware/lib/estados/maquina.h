#ifndef MAQUINA_H
#define MAQUINA_H

/*
 * Máquina de estados do robô.
 *
 * Decide, a partir de um evento, o novo estado e a ação que quem a chama deve
 * executar. Não lê HAL nem tempo: a tarefa de navegação traduz botão, comandos
 * e resultados em eventos e executa a ação devolvida.
 *
 *   HEALTH_CHECK ─ aprovado ─► AGUARDANDO ─ largar ─► RUNNING ─► SUCCESS | FAILED
 *        │                          ▲                              │
 *        └─ reprovado ─► REPROVADO  └──── health_check ◄───────────┘
 *
 * AGUARDANDO e REPROVADO saem como "health-check" na tel: o contrato
 * não tem estado novo. A largada só vale com o último health-check
 * aprovado. O comando health_check só é atendido parado, fora de tentativa.
 *
 * C puro, sem HAL: roda igual na ESP32 e nos testes do PC.
 */

#include <stdbool.h>

#include "telemetria.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MAQ_OCIOSO,        /* ligado, sem health-check: espera o comando health_check */
    MAQ_HEALTH_CHECK,  /* health-check em andamento */
    MAQ_AGUARDANDO,    /* último health-check aprovado, esperando largar */
    MAQ_REPROVADO,     /* último health-check reprovado */
    MAQ_RUNNING,
    MAQ_SUCCESS,
    MAQ_FAILED
} maq_estado_t;

typedef enum {
    MAQ_EV_HC_APROVADO,      /* o health-check terminou sem reprovação */
    MAQ_EV_HC_REPROVADO,
    MAQ_EV_CMD_LARGAR,
    MAQ_EV_CMD_HEALTH_CHECK,
    MAQ_EV_CMD_INTERROMPER,
    MAQ_EV_BOOT_TOQUE,       /* qualquer toque no BOOT; só age na corrida */
    MAQ_EV_BOOT_CURTO,       /* toque curto, robô parado */
    MAQ_EV_BOOT_LONGO,       /* toque longo, robô parado */
    MAQ_EV_CORRIDA_SUCESSO,  /* a navegação chegou */
    MAQ_EV_CORRIDA_FALHA     /* a navegação falhou (stuck, colisão, bateria...) */
} maq_evento_t;

typedef enum {
    MAQ_ACAO_NENHUMA,
    MAQ_ACAO_RODAR_HEALTH_CHECK,
    MAQ_ACAO_PARTIR,
    MAQ_ACAO_PARAR_FALHA_WEB,   /* parar e confirmar com falha origem web, sem motivo (o backend rejeita motivo aqui) */
    MAQ_ACAO_PARAR_FALHA_BOOT,  /* parar e enviar falha origem boot, motivo encerrado_operador */
    MAQ_ACAO_REPETIR_FALHA_WEB, /* já parado: só repete a confirmação */
    MAQ_ACAO_APAGAR_MAPA,
    MAQ_ACAO_APAGAR_TUDO        /* o mapa e os tipos resolvidos, com bipe duplo */
} maq_acao_t;

typedef struct {
    maq_estado_t estado;
    bool hc_aprovado; /* resultado do último health-check concluído */
} maq_t;

/* Começa em MAQ_OCIOSO: o health-check só roda quando chega o comando health_check. */
void maq_iniciar(maq_t *m);

maq_acao_t maq_evento(maq_t *m, maq_evento_t evento);

/*
 * A falha que a ação pede, na célula (x, y). Devolve false para as ações que não enviam falha.
 * Interrupção pela web: origem web, sem motivo. BOOT na corrida: origem boot, encerrado_operador.
 * Nenhuma das duas leva componente.
 */
bool maq_falha_da_acao(maq_acao_t acao, uint8_t x, uint8_t y, contrato_falha_t *falha);

/* Estado que a tel informa. */
contrato_estado_t maq_estado_contrato(const maq_t *m);

/* true só em RUNNING: as rodas só podem andar aí. */
bool maq_andando(const maq_t *m);

#ifdef __cplusplus
}
#endif

#endif /* MAQUINA_H */
