#include "tof_filtro.h"

#include <string.h>

#define MEDIANA_MAX 16

_Static_assert(TOF_MEDIANA_AMOSTRAS >= 1 && TOF_MEDIANA_AMOSTRAS <= MEDIANA_MAX,
               "TOF_MEDIANA_AMOSTRAS deve ficar entre 1 e 16");
_Static_assert(TOF_AUTOTESTE_AMOSTRAS >= 1 && TOF_AUTOTESTE_AMOSTRAS <= MEDIANA_MAX,
               "TOF_AUTOTESTE_AMOSTRAS deve ficar entre 1 e 16");
_Static_assert(TOF_FALHAS_PARA_DESCARTAR >= 1 && TOF_FALHAS_PARA_DESCARTAR <= 255,
               "TOF_FALHAS_PARA_DESCARTAR deve ficar entre 1 e 255");

void tof_filtro_iniciar(tof_filtro_t *f)
{
    memset(f, 0, sizeof(*f));
}

void tof_filtro_inserir(tof_filtro_t *f, bool respondeu, uint16_t distancia_mm)
{
    if (f->falhou) {
        return;
    }
    if (!respondeu) {
        f->falhas_seguidas++;
        if (f->falhas_seguidas >= TOF_FALHAS_PARA_DESCARTAR) {
            f->falhou = true;
        }
        return;
    }

    f->falhas_seguidas = 0;
    f->amostras[f->proxima] = tof_saturar(distancia_mm);
    f->proxima = (uint8_t)((f->proxima + 1) % TOF_MEDIANA_AMOSTRAS);
    if (f->quantidade < TOF_MEDIANA_AMOSTRAS) {
        f->quantidade++;
    }
}

bool tof_filtro_valido(const tof_filtro_t *f)
{
    return !f->falhou && f->quantidade > 0;
}

uint16_t tof_filtro_mediana(const tof_filtro_t *f)
{
    if (f->quantidade == 0) {
        return TOF_ALCANCE_MAX_MM;
    }
    return tof_mediana(f->amostras, f->quantidade);
}

uint16_t tof_saturar(uint16_t distancia_mm)
{
    return distancia_mm > TOF_ALCANCE_MAX_MM ? TOF_ALCANCE_MAX_MM : distancia_mm;
}

uint16_t tof_mediana(const uint16_t *valores, int n)
{
    uint16_t ordenados[MEDIANA_MAX];

    if (n <= 0) {
        return 0;
    }
    if (n > MEDIANA_MAX) {
        n = MEDIANA_MAX;
    }
    memcpy(ordenados, valores, (size_t)n * sizeof(uint16_t));

    /* Inserção: n é pequeno (3 a 5). */
    for (int i = 1; i < n; i++) {
        uint16_t v = ordenados[i];
        int j = i - 1;
        while (j >= 0 && ordenados[j] > v) {
            ordenados[j + 1] = ordenados[j];
            j--;
        }
        ordenados[j + 1] = v;
    }

    /* Com n par, a média dos dois do meio. */
    if (n % 2 == 0) {
        return (uint16_t)((ordenados[n / 2 - 1] + ordenados[n / 2]) / 2);
    }
    return ordenados[n / 2];
}
