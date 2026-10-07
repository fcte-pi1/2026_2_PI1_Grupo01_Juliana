#include "tof_autoteste.h"

#include "config/robo.h"
#include "tof_filtro.h"

bool tof_autoteste_avaliar(const bool respondeu[], const uint16_t distancia_mm[], int n,
                           uint16_t *valor_mm)
{
    uint16_t validas[TOF_AUTOTESTE_AMOSTRAS];
    int quantidade = 0;

    for (int i = 0; i < n && quantidade < TOF_AUTOTESTE_AMOSTRAS; i++) {
        if (respondeu[i]) {
            validas[quantidade++] = tof_saturar(distancia_mm[i]);
        }
    }

    *valor_mm = quantidade > 0 ? tof_mediana(validas, quantidade) : 0;
    return quantidade == n && *valor_mm >= TOF_AUTOTESTE_MIN_MM;
}

void tof_autoteste_executar(tof_autoteste_t resultado[TOF_QTD])
{
    for (int id = 0; id < TOF_QTD; id++) {
        bool respondeu[TOF_AUTOTESTE_AMOSTRAS];
        uint16_t distancia[TOF_AUTOTESTE_AMOSTRAS];

        for (int i = 0; i < TOF_AUTOTESTE_AMOSTRAS; i++) {
            distancia[i] = 0;
            respondeu[i] = hal_tof_ler_mm((tof_id_t)id, &distancia[i]);
        }

        resultado[id].nome = hal_tof_nome((tof_id_t)id);
        resultado[id].ok = tof_autoteste_avaliar(respondeu, distancia, TOF_AUTOTESTE_AMOSTRAS,
                                                 &resultado[id].valor_mm);
    }
}
