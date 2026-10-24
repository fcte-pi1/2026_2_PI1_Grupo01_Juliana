#if defined(ARDUINO)

/*
 * NVS da ESP32 (flash). Vale também com HAL_SIMULADA: a NVS em RAM não
 * sobrevive ao reset da placa, e a retomada (FIRM-08) precisa sobreviver.
 * No teste nativo, quem implementa a HAL é hal_nvs_sim.c.
 */

#include <Preferences.h>
#include <string.h>

#include "hal_nvs.h"

#define NVS_NAMESPACE "ratatouille"
#define NVS_MAX_CHAVE 15
#define NVS_MAX_BLOCO 2048

static Preferences prefs;
static bool aberta;

bool hal_nvs_iniciar(void)
{
    aberta = prefs.begin(NVS_NAMESPACE, false);
    return aberta;
}

static bool chave_aceita(const char *chave, size_t tamanho)
{
    return aberta && chave != nullptr && strlen(chave) <= NVS_MAX_CHAVE && tamanho > 0 &&
           tamanho <= NVS_MAX_BLOCO;
}

bool hal_nvs_gravar(const char *chave, const void *dados, size_t tamanho)
{
    if (!chave_aceita(chave, tamanho) || dados == nullptr) {
        return false;
    }
    return prefs.putBytes(chave, dados, tamanho) == tamanho;
}

bool hal_nvs_ler(const char *chave, void *dados, size_t tamanho)
{
    if (!chave_aceita(chave, tamanho) || dados == nullptr) {
        return false;
    }
    if (prefs.getBytesLength(chave) != tamanho) {
        return false;
    }
    return prefs.getBytes(chave, dados, tamanho) == tamanho;
}

bool hal_nvs_apagar(const char *chave)
{
    if (!aberta || chave == nullptr || !prefs.isKey(chave)) {
        return false;
    }
    return prefs.remove(chave);
}

#endif /* ARDUINO */
