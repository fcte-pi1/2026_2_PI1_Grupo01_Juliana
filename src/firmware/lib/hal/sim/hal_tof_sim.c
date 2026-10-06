#ifdef HAL_SIMULADA

#include "hal_tof.h"
#include "sim_mundo.h"

static const char *const NOMES[TOF_QTD] = {
    "tof_frontal_esq",
    "tof_frontal_dir",
    "tof_esquerdo",
    "tof_direito",
};

const char *hal_tof_nome(tof_id_t id)
{
    return (id >= 0 && id < TOF_QTD) ? NOMES[id] : "tof_desconhecido";
}

bool hal_tof_iniciar(void)
{
    bool todos_responderam = true;
    uint16_t descartada;

    for (int i = 0; i < TOF_QTD; i++) {
        if (!sim_tof_medir(i, &descartada)) {
            todos_responderam = false;
        }
    }
    return todos_responderam;
}

bool hal_tof_ler_mm(tof_id_t id, uint16_t *distancia_mm)
{
    if (id < 0 || id >= TOF_QTD) {
        return false;
    }
    return sim_tof_medir(id, distancia_mm);
}

#endif /* HAL_SIMULADA */
