#include "memoria.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hal_nvs.h"

/* Enums gravados como uint8_t para o layout não depender do compilador. */
typedef struct {
    uint16_t versao;
    int8_t x;
    int8_t y;
    uint8_t direcao;
    uint8_t estado;
    uint8_t mapa_alterado;
    uint8_t movimento_pendente;
    uint8_t perdoes;
    uint8_t bloqueios;
    uint8_t terminou;
    uint8_t acao_terminal;
    nav_celula_t mapa[NAV_MAX_CELULAS][NAV_MAX_CELULAS];
    nav_parede_t paredes[NAV_MAX_CELULAS][NAV_MAX_CELULAS][2];
    uint32_t crc;
} registro_mapa_t;

typedef struct {
    uint16_t versao;
    uint8_t tipo;
    uint8_t orientacao;
    uint8_t lado;
    uint8_t lado_provisorio;
    int8_t lado_y;
    uint8_t lado_duas_abertas;
    uint8_t desfazeres;
    uint32_t crc;
} registro_tipos_t;

/* CRC-32 (o do zlib), bit a bit: o registro tem ~1,2 KB, não compensa tabela. */
static uint32_t crc32(const void *dados, size_t tamanho)
{
    const uint8_t *p = (const uint8_t *)dados;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < tamanho; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/* O CRC cobre tudo antes do campo crc, inclusive o padding (zerado no memset). */
#define CRC_MAPA(r)  crc32((r), offsetof(registro_mapa_t, crc))
#define CRC_TIPOS(r) crc32((r), offsetof(registro_tipos_t, crc))

static void montar_tipos(registro_tipos_t *r, const nav_estado_t *nav)
{
    memset(r, 0, sizeof(*r));
    r->versao = MEM_VERSAO;
    r->tipo = (uint8_t)nav->tipo;
    r->orientacao = (uint8_t)nav->orientacao;
    r->lado = (uint8_t)nav->lado;
    r->lado_provisorio = nav->lado_provisorio;
    r->lado_y = nav->lado_y;
    r->lado_duas_abertas = nav->lado_duas_abertas;
    r->desfazeres = nav->desfazeres;
    r->crc = CRC_TIPOS(r);
}

bool mem_gravar(const nav_estado_t *nav, contrato_estado_t estado)
{
    /* static: o registro tem ~1,2 KB e a pilha da tarefa é de 4 KB. */
    static registro_mapa_t mapa;
    memset(&mapa, 0, sizeof(mapa));
    mapa.versao = MEM_VERSAO;
    mapa.x = nav->x;
    mapa.y = nav->y;
    mapa.direcao = (uint8_t)nav->direcao;
    mapa.estado = (uint8_t)estado;
    mapa.mapa_alterado = nav->mapa_alterado;
    mapa.movimento_pendente = nav->movimento_pendente;
    mapa.perdoes = nav->perdoes;
    mapa.bloqueios = nav->bloqueios;
    mapa.terminou = nav->terminou;
    mapa.acao_terminal = (uint8_t)nav->acao_terminal;
    memcpy(mapa.mapa, nav->mapa, sizeof(mapa.mapa));
    memcpy(mapa.paredes, nav->paredes, sizeof(mapa.paredes));
    mapa.crc = CRC_MAPA(&mapa);

    registro_tipos_t tipos, gravado;
    montar_tipos(&tipos, nav);
    bool tipos_ok = true;
    if (!hal_nvs_ler(MEM_CHAVE_TIPOS, &gravado, sizeof(gravado)) ||
        memcmp(&gravado, &tipos, sizeof(tipos)) != 0) {
        tipos_ok = hal_nvs_gravar(MEM_CHAVE_TIPOS, &tipos, sizeof(tipos));
    }
    return hal_nvs_gravar(MEM_CHAVE_MAPA, &mapa, sizeof(mapa)) && tipos_ok;
}

static bool ler_tipos(registro_tipos_t *r, nav_tipo_t tipo_dip)
{
    return hal_nvs_ler(MEM_CHAVE_TIPOS, r, sizeof(*r)) && r->versao == MEM_VERSAO &&
           r->crc == CRC_TIPOS(r) && r->tipo == (uint8_t)tipo_dip;
}

static bool ler_mapa(registro_mapa_t *r)
{
    return hal_nvs_ler(MEM_CHAVE_MAPA, r, sizeof(*r)) && r->versao == MEM_VERSAO &&
           r->crc == CRC_MAPA(r);
}

mem_inicio_t mem_carregar(nav_estado_t *nav, contrato_estado_t *estado, nav_tipo_t tipo_dip)
{
    nav_iniciar(nav, tipo_dip);
    *estado = CONTRATO_ESTADO_HEALTH_CHECK;

    registro_tipos_t tipos;
    if (!ler_tipos(&tipos, tipo_dip)) {
        return MEM_NOVA;
    }
    nav->orientacao = (nav_orientacao_t)tipos.orientacao;
    nav->lado = (nav_lado_t)tipos.lado;
    nav->lado_provisorio = tipos.lado_provisorio;
    nav->lado_y = tipos.lado_y;
    nav->lado_duas_abertas = tipos.lado_duas_abertas;
    nav->desfazeres = tipos.desfazeres;

    static registro_mapa_t mapa;
    if (!ler_mapa(&mapa)) {
        /* A orientação muda a região válida: as distâncias do mapa vazio precisam ser refeitas. */
        nav_recalcular(nav);
        return MEM_SO_TIPOS;
    }
    nav->x = mapa.x;
    nav->y = mapa.y;
    nav->direcao = (nav_direcao_t)mapa.direcao;
    nav->mapa_alterado = mapa.mapa_alterado;
    nav->movimento_pendente = mapa.movimento_pendente;
    nav->perdoes = mapa.perdoes;
    nav->bloqueios = mapa.bloqueios;
    nav->terminou = mapa.terminou;
    nav->acao_terminal = (nav_acao_t)mapa.acao_terminal;
    memcpy(nav->mapa, mapa.mapa, sizeof(nav->mapa));
    memcpy(nav->paredes, mapa.paredes, sizeof(nav->paredes));
    *estado = (contrato_estado_t)mapa.estado;
    return MEM_RETOMADA;
}

void mem_apagar_mapa(void) { hal_nvs_apagar(MEM_CHAVE_MAPA); }

void mem_apagar_tudo(void)
{
    hal_nvs_apagar(MEM_CHAVE_MAPA);
    hal_nvs_apagar(MEM_CHAVE_TIPOS);
}
