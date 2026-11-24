#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

/*
 * Health-check dos 8 componentes fixos.
 *
 * Junta a leitura da bateria, o autoteste dos ToF e o giro das rodas em um resultado, sem publicar nada: quem chama envia os
 * hc_item e o hc_resultado. Roda no boot e quando o operador pede um novo.
 *
 * Reprovação não gera `falha`: vai só no hc_item do componente e no
 * hc_resultado. O primeiro componente reprovado fica em `reprovado`, na ordem
 * da lista.
 *
 * Motor e encoder passam pelo autoteste de giro (motor_autoteste.h): o encoder
 * leva os pulsos contados e o motor é aprovado pelo encoder da mesma roda.
 * O tipo do labirinto vem do DIP switch; o código 11 reprova o resultado sem
 * reprovar nenhum componente (`dip_valido` = false, `reprovado` = NENHUM).
 */

#include <stdbool.h>
#include <stdint.h>

#include "bateria.h"
#include "motor_autoteste.h"
#include "telemetria.h"
#include "telemetria_envio.h"
#include "tof_autoteste.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HC_ITENS 8
#define HC_EVENTOS (HC_ITENS + 1) /* um hc_item por componente e o hc_resultado */

typedef struct {
    contrato_hc_item_t item[HC_ITENS]; /* bateria, 3 ToF, 2 motores, 2 encoders */
    bool aprovado;
    contrato_componente_t reprovado;   /* primeiro item reprovado; NENHUM se nenhum reprovou */
    contrato_tipo_dip_t tipo_dip;      /* lido nas vias 1 e 2 do DIP */
    bool dip_valido;
} hc_resultado_t;

/* Parte pura: avalia as medidas já feitas. Os testes passam valores sintéticos. */
void hc_avaliar(uint16_t bateria_mv, bateria_nivel_t bateria_nivel,
                const tof_autoteste_t tof[TOF_QTD], const int32_t pulsos[MOTOR_QTD],
                contrato_tipo_dip_t tipo_dip, hc_resultado_t *resultado);

/* Transforma o resultado nos eventos do contrato, na ordem de envio. Devolve HC_EVENTOS. */
int hc_montar_eventos(const hc_resultado_t *resultado, contrato_inicio_t inicio,
                      telemetria_evento_t eventos[HC_EVENTOS]);

/*
 * Mede pela HAL (bateria, ToF, giro das rodas e DIP) e avalia. As rodas giram
 * por cerca de 1 s no total: rodas no ar e interruptor da bateria ligado.
 */
void hc_executar(bateria_t *bateria, hc_esperar_t esperar, hc_resultado_t *resultado);

#ifdef __cplusplus
}
#endif

#endif /* HEALTH_CHECK_H */
