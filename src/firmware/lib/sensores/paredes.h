#ifndef PAREDES_H
#define PAREDES_H

/*
 * Detecção de parede a partir dos 4 ToF filtrados (FIRM-03, #129; US33, UC24).
 *
 * C puro: recebe os filtros (tof_filtro.h) e devolve frente/esquerda/direita.
 *
 *   - frente: média dos frontais válidos; com um só válido, usa esse;
 *   - esquerda/direita: o lateral daquele lado;
 *   - histerese: vira parede abaixo de *_ENTRA_MM e só deixa de ser acima de
 *     *_SAI_MM (config/robo.h); entre os dois, mantém o estado anterior;
 *   - sensor falho ou sem leitura conta como "sem parede".
 *
 * ATENÇÃO: o lateral a 45° mede a parede ~74 mm à frente do robô. A lateral de
 * uma célula só é confiável perto da entrada dela (de ~65 mm antes a ~30 mm
 * depois da fronteira; ver config/robo.h). Quem usa o resultado lateral deve
 * guardá-lo ao entrar na célula (FIRM-05 #135 / FIRM-01 #117).
 */

#include <stdbool.h>
#include <stdint.h>

#include "hal_tof.h"
#include "tof_filtro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool frente;
    bool esquerda;
    bool direita;
} paredes_t;

void paredes_iniciar(paredes_t *estado);

/* Atualiza o estado com as leituras filtradas e devolve o novo estado. */
paredes_t paredes_atualizar(paredes_t *estado, const tof_filtro_t filtros[TOF_QTD]);

/* Histerese de um lado: decide se há parede dado o estado anterior. */
bool paredes_histerese(bool havia_parede, uint16_t distancia_mm, uint16_t entra_mm,
                       uint16_t sai_mm);

#ifdef __cplusplus
}
#endif

#endif /* PAREDES_H */
