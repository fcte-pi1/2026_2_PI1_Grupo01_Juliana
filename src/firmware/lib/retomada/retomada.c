#include "retomada.h"

#include <stddef.h>
#include <string.h>

#include "hal_nvs.h"

#define RETOMADA_VERSAO 1
#define RETOMADA_CHAVE "mapa"

/*
 * Estático de propósito: o registro acompanha o nav_estado_t (~1,2 KB) e a
 * tarefa de navegação tem pilha de 4 KB. Só essa tarefa chama a gravação.
 */
typedef struct {
    uint8_t versao;
    uint32_t crc;
    nav_estado_t estado;
} registro_t;

static registro_t registro;

_Static_assert(sizeof(registro_t) <= 2048, "registro do mapa maior que a NVS simulada");

static uint32_t crc32(const void *dados, size_t tamanho)
{
    const unsigned char *p = dados;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < tamanho; i++) {
        crc ^= p[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mascara = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mascara);
        }
    }
    return ~crc;
}

static void mapa_vazio(nav_estado_t *estado, nav_tipo_t tipo_dip)
{
    nav_iniciar(estado, tipo_dip);
}

bool retomada_gravar(const nav_estado_t *estado)
{
    memset(&registro, 0, sizeof(registro));
    registro.versao = RETOMADA_VERSAO;
    registro.estado = *estado;
    registro.crc = crc32(&registro, sizeof(registro));
    return hal_nvs_gravar(RETOMADA_CHAVE, &registro, sizeof(registro));
}

bool retomada_carregar(nav_estado_t *estado, nav_tipo_t tipo_dip)
{
    uint32_t crc_gravado;

    if (!hal_nvs_ler(RETOMADA_CHAVE, &registro, sizeof(registro))) {
        mapa_vazio(estado, tipo_dip);
        return false;
    }

    crc_gravado = registro.crc;
    registro.crc = 0;
    if (registro.versao != RETOMADA_VERSAO || crc32(&registro, sizeof(registro)) != crc_gravado) {
        mapa_vazio(estado, tipo_dip);
        return false;
    }
    if (registro.estado.tipo != tipo_dip) {
        mapa_vazio(estado, tipo_dip);
        return false;
    }

    *estado = registro.estado;
    return true;
}

void retomada_sessao_iniciar(retomada_sessao_t *sessao)
{
    sessao->x = 0;
    sessao->y = 0;
    sessao->gravou = false;
}

bool retomada_na_celula(retomada_sessao_t *sessao, const nav_estado_t *estado)
{
    if (sessao->gravou && sessao->x == estado->x && sessao->y == estado->y) {
        return true;
    }
    if (!retomada_gravar(estado)) {
        return false;
    }
    sessao->x = estado->x;
    sessao->y = estado->y;
    sessao->gravou = true;
    return true;
}
