#if !defined(HAL_SIMULADA) && defined(TOF_MOCK) && defined(ARDUINO)

/*
 * ToF mockado para a placa real (compilado com -D TOF_MOCK): leituras fixas, para o
 * health-check aprovar enquanto algum sensor ainda não funciona. Não mede nada.
 */

#include "hal_tof.h"

#define MOCK_FRONTAL_MM 300
#define MOCK_LATERAL_MM 120

const char *hal_tof_nome(tof_id_t id)
{
    static const char *const NOMES[TOF_QTD] = {"tof_frontal", "tof_esquerdo", "tof_direito"};
    return (id >= 0 && id < TOF_QTD) ? NOMES[id] : "tof_desconhecido";
}

bool hal_tof_iniciar(void) { return true; }

bool hal_tof_ler_mm(tof_id_t id, uint16_t *distancia_mm)
{
    if (id < 0 || id >= TOF_QTD) {
        return false;
    }
    *distancia_mm = id == TOF_FRONTAL ? MOCK_FRONTAL_MM : MOCK_LATERAL_MM;
    return true;
}

#endif
