/*
 * Mapa e estado na NVS — FIRM-08 (#137). Critérios de aceite: reset no meio do
 * trajeto volta com o mesmo mapa e a mesma célula; registro corrompido vira
 * mapa vazio.
 */

#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "hal_nvs.h"
#include "memoria.h"
#include "nav.h"
#include "sim/hal_sim.h"

static nav_estado_t gravado;
static nav_estado_t lido;

/* Corredor com uma parede de vez em quando, para o mapa ter votos dos dois tipos. */
static const nav_leitura_t TRAJETO[] = {
    {false, true, true}, {false, true, false}, {true, false, true},
    {false, false, true}, {true, true, false}, {false, true, true},
};
#define PASSOS ((int)(sizeof(TRAJETO) / sizeof(TRAJETO[0])))

static void andar(nav_estado_t *nav, int de, int ate)
{
    for (int i = de; i < ate; i++) {
        nav_passo(nav, TRAJETO[i]);
    }
}

/* Estado depois de 3 células, já com orientação e lado resolvidos. */
static void montar_8x4(nav_estado_t *nav)
{
    nav_iniciar(nav, NAV_TIPO_8X4);
    andar(nav, 0, 3);
    nav->orientacao = NAV_ORIENTACAO_LONGO_Y;
    nav->lado = NAV_LADO_ESQUERDA;
    nav->desfazeres = 2;
    nav_recalcular(nav);
}

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

/* Troca a versão (primeiros 2 bytes) e refaz o CRC (últimos 4), para só a versão estar errada. */
static void trocar_versao(const char *chave)
{
    static uint8_t bloco[2048];
    size_t n = hal_nvs_sim_ler_bruto(chave, bloco, sizeof(bloco));
    TEST_ASSERT_TRUE(n > 6);
    bloco[0] = (uint8_t)(MEM_VERSAO + 1);
    bloco[1] = 0;
    uint32_t crc = crc32(bloco, n - 4);
    memcpy(&bloco[n - 4], &crc, sizeof(crc));
    TEST_ASSERT_TRUE(hal_nvs_gravar(chave, bloco, n));
}

static void estragar_byte(const char *chave, size_t posicao)
{
    static uint8_t bloco[2048];
    size_t n = hal_nvs_sim_ler_bruto(chave, bloco, sizeof(bloco));
    TEST_ASSERT_TRUE(n > posicao);
    bloco[posicao] ^= 0x5A;
    TEST_ASSERT_TRUE(hal_nvs_gravar(chave, bloco, n));
}

/* Mapa vazio, mas com tipo, orientação e lado do estado gravado. */
static void conferir_mapa_vazio_com_tipos(const nav_estado_t *nav, const nav_estado_t *tipos)
{
    nav_estado_t vazio;
    nav_iniciar(&vazio, tipos->tipo);
    TEST_ASSERT_EQUAL_INT8(0, nav->x);
    TEST_ASSERT_EQUAL_INT8(0, nav->y);
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav->direcao);
    TEST_ASSERT_EQUAL_MEMORY(vazio.paredes, nav->paredes, sizeof(vazio.paredes));
    TEST_ASSERT_EQUAL_INT(tipos->orientacao, nav->orientacao);
    TEST_ASSERT_EQUAL_INT(tipos->lado, nav->lado);
    TEST_ASSERT_EQUAL_UINT8(tipos->desfazeres, nav->desfazeres);
}

void setUp(void)
{
    hal_nvs_sim_apagar_tudo();
    hal_nvs_iniciar();
}

void tearDown(void) {}

static void test_gravou_e_carregou_igual(void)
{
    montar_8x4(&gravado);
    TEST_ASSERT_TRUE(mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING));

    contrato_estado_t estado = CONTRATO_ESTADO_SUCCESS;
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, estado);
    TEST_ASSERT_EQUAL_MEMORY(&gravado, &lido, sizeof(gravado));
}

static void test_sem_nada_gravado_comeca_vazio(void)
{
    nav_estado_t vazio;
    nav_iniciar(&vazio, NAV_TIPO_4X4);
    contrato_estado_t estado = CONTRATO_ESTADO_RUNNING;

    TEST_ASSERT_EQUAL_INT(MEM_NOVA, mem_carregar(&lido, &estado, NAV_TIPO_4X4));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, estado);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &lido, sizeof(vazio));
}

static void test_reset_no_meio_volta_na_mesma_celula_e_segue_igual(void)
{
    nav_iniciar(&gravado, NAV_TIPO_4X4);
    andar(&gravado, 0, 3);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);

    /* reset: a NVS simulada sobrevive a um novo hal_nvs_iniciar */
    hal_nvs_iniciar();
    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA, mem_carregar(&lido, &estado, NAV_TIPO_4X4));
    TEST_ASSERT_EQUAL_INT8(gravado.x, lido.x);
    TEST_ASSERT_EQUAL_INT8(gravado.y, lido.y);
    TEST_ASSERT_EQUAL_INT(gravado.direcao, lido.direcao);

    for (int i = 3; i < PASSOS; i++) {
        TEST_ASSERT_EQUAL_INT(nav_passo(&gravado, TRAJETO[i]), nav_passo(&lido, TRAJETO[i]));
    }
    TEST_ASSERT_EQUAL_MEMORY(&gravado, &lido, sizeof(gravado));
}

static void test_crc_estragado_no_mapa_vira_mapa_vazio(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    estragar_byte(MEM_CHAVE_MAPA, 100);

    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_SO_TIPOS, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, estado);
    conferir_mapa_vazio_com_tipos(&lido, &gravado);
}

static void test_crc_estragado_nos_tipos_vira_tudo_vazio(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    estragar_byte(MEM_CHAVE_TIPOS, 3);

    nav_estado_t vazio;
    nav_iniciar(&vazio, NAV_TIPO_8X4);
    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_NOVA, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &lido, sizeof(vazio));
}

static void test_versao_diferente_vira_mapa_vazio(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    trocar_versao(MEM_CHAVE_MAPA);

    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_SO_TIPOS, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    conferir_mapa_vazio_com_tipos(&lido, &gravado);

    trocar_versao(MEM_CHAVE_TIPOS);
    TEST_ASSERT_EQUAL_INT(MEM_NOVA, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
}

static void test_tipo_do_dip_diferente_vira_mapa_vazio(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);

    nav_estado_t vazio;
    nav_iniciar(&vazio, NAV_TIPO_12X4);
    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_NOVA, mem_carregar(&lido, &estado, NAV_TIPO_12X4));
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &lido, sizeof(vazio));
}

static void test_apagar_mapa_preserva_orientacao_e_lado(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    mem_apagar_mapa();

    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_SO_TIPOS, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    conferir_mapa_vazio_com_tipos(&lido, &gravado);
}

static void test_apagar_tudo_apaga_os_dois(void)
{
    montar_8x4(&gravado);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    mem_apagar_tudo();

    nav_estado_t vazio;
    nav_iniciar(&vazio, NAV_TIPO_8X4);
    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_NOVA, mem_carregar(&lido, &estado, NAV_TIPO_8X4));
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &lido, sizeof(vazio));
}

static void test_gravar_de_novo_fica_com_o_ultimo(void)
{
    nav_iniciar(&gravado, NAV_TIPO_4X4);
    andar(&gravado, 0, 2);
    mem_gravar(&gravado, CONTRATO_ESTADO_RUNNING);
    andar(&gravado, 2, 4);
    mem_gravar(&gravado, CONTRATO_ESTADO_FAILED);

    contrato_estado_t estado;
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA, mem_carregar(&lido, &estado, NAV_TIPO_4X4));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, estado);
    TEST_ASSERT_EQUAL_MEMORY(&gravado, &lido, sizeof(gravado));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gravou_e_carregou_igual);
    RUN_TEST(test_sem_nada_gravado_comeca_vazio);
    RUN_TEST(test_reset_no_meio_volta_na_mesma_celula_e_segue_igual);
    RUN_TEST(test_crc_estragado_no_mapa_vira_mapa_vazio);
    RUN_TEST(test_crc_estragado_nos_tipos_vira_tudo_vazio);
    RUN_TEST(test_versao_diferente_vira_mapa_vazio);
    RUN_TEST(test_tipo_do_dip_diferente_vira_mapa_vazio);
    RUN_TEST(test_apagar_mapa_preserva_orientacao_e_lado);
    RUN_TEST(test_apagar_tudo_apaga_os_dois);
    RUN_TEST(test_gravar_de_novo_fica_com_o_ultimo);
    return UNITY_END();
}
