#if defined(HAL_SIMULADA) && !defined(ARDUINO)

/*
 * NVS simulada em RAM. Sobrevive a um "reboot" simulado (o firmware chamar
 * hal_nvs_iniciar de novo), mas não a desligar a ESP32 nem a fechar o teste.
 */

#include <string.h>

#include "hal_nvs.h"
#include "hal_sim.h"

#define MAX_REGISTROS   8
#define MAX_CHAVE       16
#define MAX_TAMANHO     2048 /* o registro do mapa (FIRM-08) tem ~1,2 KB */

typedef struct {
    bool usado;
    char chave[MAX_CHAVE];
    size_t tamanho;
    unsigned char dados[MAX_TAMANHO];
} registro_t;

static registro_t registros[MAX_REGISTROS];

static registro_t *buscar(const char *chave)
{
    for (int i = 0; i < MAX_REGISTROS; i++) {
        if (registros[i].usado && strcmp(registros[i].chave, chave) == 0) {
            return &registros[i];
        }
    }
    return NULL;
}

bool hal_nvs_iniciar(void) { return true; }

bool hal_nvs_gravar(const char *chave, const void *dados, size_t tamanho)
{
    if (strlen(chave) >= MAX_CHAVE || tamanho > MAX_TAMANHO) {
        return false;
    }

    registro_t *r = buscar(chave);
    for (int i = 0; r == NULL && i < MAX_REGISTROS; i++) {
        if (!registros[i].usado) {
            r = &registros[i];
        }
    }
    if (r == NULL) {
        return false;
    }

    r->usado = true;
    strcpy(r->chave, chave);
    r->tamanho = tamanho;
    memcpy(r->dados, dados, tamanho);
    return true;
}

bool hal_nvs_ler(const char *chave, void *dados, size_t tamanho)
{
    registro_t *r = buscar(chave);
    if (r == NULL || r->tamanho != tamanho) {
        return false;
    }
    memcpy(dados, r->dados, tamanho);
    return true;
}

bool hal_nvs_apagar(const char *chave)
{
    registro_t *r = buscar(chave);
    if (r == NULL) {
        return false;
    }
    r->usado = false;
    return true;
}

void hal_nvs_sim_apagar_tudo(void) { memset(registros, 0, sizeof(registros)); }

size_t hal_nvs_sim_ler_bruto(const char *chave, void *dados, size_t capacidade)
{
    registro_t *r = buscar(chave);
    if (r == NULL || r->tamanho > capacidade) {
        return 0;
    }
    memcpy(dados, r->dados, r->tamanho);
    return r->tamanho;
}

#endif /* HAL_SIMULADA && !ARDUINO */
