#include "sensores.h"

#include "tof_filtro.h"

static tof_filtro_t filtros[TOF_QTD];
static paredes_t paredes;

void sensores_iniciar(void)
{
    for (int i = 0; i < TOF_QTD; i++) {
        tof_filtro_iniciar(&filtros[i]);
    }
    paredes_iniciar(&paredes);
}

void sensores_ler(void)
{
    for (int i = 0; i < TOF_QTD; i++) {
        uint16_t distancia = 0;
        bool respondeu = hal_tof_ler_mm((tof_id_t)i, &distancia);
        tof_filtro_inserir(&filtros[i], respondeu, distancia);
    }
    paredes_atualizar(&paredes, filtros);
}

paredes_t sensores_paredes(void)
{
    return paredes;
}

bool sensores_distancia_mm(tof_id_t id, uint16_t *distancia_mm)
{
    if (id < 0 || id >= TOF_QTD || !tof_filtro_valido(&filtros[id])) {
        return false;
    }
    *distancia_mm = tof_filtro_mediana(&filtros[id]);
    return true;
}

bool sensores_tof_falhou(tof_id_t id)
{
    return id < 0 || id >= TOF_QTD || filtros[id].falhou;
}
