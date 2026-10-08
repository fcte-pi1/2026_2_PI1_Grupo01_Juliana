#include "paredes.h"

#include "config/robo.h"

_Static_assert(PAREDE_FRENTE_ENTRA_MM <= PAREDE_FRENTE_SAI_MM,
               "histerese da frente: ENTRA deve ser <= SAI");
_Static_assert(PAREDE_LADO_ENTRA_MM <= PAREDE_LADO_SAI_MM,
               "histerese lateral: ENTRA deve ser <= SAI");

void paredes_iniciar(paredes_t *estado)
{
    estado->frente = false;
    estado->esquerda = false;
    estado->direita = false;
}

bool paredes_histerese(bool havia_parede, uint16_t distancia_mm, uint16_t entra_mm,
                       uint16_t sai_mm)
{
    if (distancia_mm < entra_mm) {
        return true;
    }
    if (distancia_mm > sai_mm) {
        return false;
    }
    return havia_parede;
}

static bool lado(bool havia_parede, const tof_filtro_t *f)
{
    if (!tof_filtro_valido(f)) {
        return false;
    }
    return paredes_histerese(havia_parede, tof_filtro_mediana(f), PAREDE_LADO_ENTRA_MM,
                             PAREDE_LADO_SAI_MM);
}

static bool frente(bool havia_parede, const tof_filtro_t *f)
{
    if (!tof_filtro_valido(f)) {
        return false;
    }
    return paredes_histerese(havia_parede, tof_filtro_mediana(f), PAREDE_FRENTE_ENTRA_MM,
                             PAREDE_FRENTE_SAI_MM);
}

paredes_t paredes_atualizar(paredes_t *estado, const tof_filtro_t filtros[TOF_QTD])
{
    estado->frente = frente(estado->frente, &filtros[TOF_FRONTAL]);
    estado->esquerda = lado(estado->esquerda, &filtros[TOF_ESQUERDO]);
    estado->direita = lado(estado->direita, &filtros[TOF_DIREITO]);
    return *estado;
}
