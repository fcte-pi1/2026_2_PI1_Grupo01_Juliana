/*
 * Mapa na NVS — FIRM-08 (#137). Critérios de aceite:
 *   - reset no meio do trajeto volta com o mesmo mapa e a mesma célula;
 *   - registro corrompido é detectado pelo CRC e vira mapa vazio;
 *   - a gravação leva menos de 20 ms e não altera o estado do robô.
 */

#include <string.h>
#include <time.h>
#include <unity.h>

#include "hal_nvs.h"
#include "nav.h"
#include "retomada.h"
#include "sim/hal_sim.h"

void setUp(void)
{
    hal_nvs_sim_apagar_tudo();
    hal_nvs_iniciar();
}

void tearDown(void) {}

static uint32_t agora_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000u);
}

/* Anda algumas células com a esquerda fechada, para o mapa deixar de ser o da largada. */
static void andar(nav_estado_t *nav, int passos)
{
    nav_leitura_t leitura;
    leitura.frente = false;
    leitura.esquerda = true;
    leitura.direita = false;

    for (int i = 0; i < passos && !nav->terminou; i++) {
        nav_passo(nav, leitura);
    }
}

static void test_reset_no_meio_do_trajeto_volta_a_mesma_celula(void)
{
    nav_estado_t nav;
    nav_estado_t esperado;
    nav_leitura_t leitura;
    nav_acao_t no_original;
    nav_acao_t na_retomada;

    nav_iniciar(&nav, NAV_TIPO_4X4);
    andar(&nav, 4);
    TEST_ASSERT_TRUE(nav.x != 0 || nav.y != 0);

    TEST_ASSERT_TRUE(retomada_gravar(&nav));
    esperado = nav;

    memset(&nav, 0xA5, sizeof(nav));
    hal_nvs_iniciar(); /* reboot: a NVS continua */

    TEST_ASSERT_TRUE(retomada_carregar(&nav, NAV_TIPO_4X4));
    TEST_ASSERT_EQUAL_INT8(esperado.x, nav.x);
    TEST_ASSERT_EQUAL_INT8(esperado.y, nav.y);
    TEST_ASSERT_EQUAL_INT(esperado.direcao, nav.direcao);
    TEST_ASSERT_EQUAL_INT(esperado.orientacao, nav.orientacao);
    TEST_ASSERT_EQUAL_INT(esperado.tipo, nav.tipo);
    TEST_ASSERT_EQUAL_MEMORY(&esperado, &nav, sizeof(nav));

    leitura.frente = false;
    leitura.esquerda = true;
    leitura.direita = false;
    no_original = nav_passo(&esperado, leitura);
    na_retomada = nav_passo(&nav, leitura);
    TEST_ASSERT_EQUAL_INT(no_original, na_retomada);
    TEST_ASSERT_EQUAL_INT8(esperado.x, nav.x);
    TEST_ASSERT_EQUAL_INT8(esperado.y, nav.y);
}

static void test_crc_errado_vira_mapa_vazio(void)
{
    nav_estado_t nav;
    nav_estado_t vazio;

    nav_iniciar(&nav, NAV_TIPO_4X4);
    andar(&nav, 4);
    TEST_ASSERT_TRUE(retomada_gravar(&nav));

    /* Um byte no meio do mapa. A versão continua 1; só o CRC não fecha. */
    hal_nvs_sim_corromper("mapa", 40);

    memset(&nav, 0xA5, sizeof(nav));
    TEST_ASSERT_FALSE(retomada_carregar(&nav, NAV_TIPO_4X4));

    nav_iniciar(&vazio, NAV_TIPO_4X4);
    TEST_ASSERT_EQUAL_INT8(0, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &nav, sizeof(nav));
}

static void test_crc_do_cabecalho_errado_vira_mapa_vazio(void)
{
    nav_estado_t nav;
    nav_estado_t vazio;

    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.x = 2;
    nav.y = 1;
    TEST_ASSERT_TRUE(retomada_gravar(&nav));
    hal_nvs_sim_corromper("mapa", 0);

    TEST_ASSERT_FALSE(retomada_carregar(&nav, NAV_TIPO_8X4));
    nav_iniciar(&vazio, NAV_TIPO_8X4);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &nav, sizeof(nav));
}

static void test_sem_registro_vira_mapa_vazio(void)
{
    nav_estado_t nav;
    nav_estado_t vazio;

    memset(&nav, 0xA5, sizeof(nav));
    TEST_ASSERT_FALSE(retomada_carregar(&nav, NAV_TIPO_12X4));
    nav_iniciar(&vazio, NAV_TIPO_12X4);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &nav, sizeof(nav));
}

static void test_tipo_diferente_do_dip_zera_o_mapa(void)
{
    nav_estado_t nav;
    nav_estado_t vazio;

    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.x = 3;
    nav.y = 1;
    nav.direcao = NAV_LESTE;
    nav.orientacao = NAV_ORIENTACAO_LONGO_X;
    nav.lado = NAV_LADO_DIREITA;
    TEST_ASSERT_TRUE(retomada_gravar(&nav));

    TEST_ASSERT_FALSE(retomada_carregar(&nav, NAV_TIPO_4X4));
    nav_iniciar(&vazio, NAV_TIPO_4X4);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &nav, sizeof(nav));
}

static void test_gravacao_cabe_em_20_ms_e_nao_mexe_no_estado(void)
{
    nav_estado_t nav;
    nav_estado_t antes;
    uint32_t pior_us = 0;

    nav_iniciar(&nav, NAV_TIPO_12X4);
    andar(&nav, 6);
    antes = nav;

    for (int i = 0; i < 10; i++) {
        uint32_t inicio = agora_us();
        TEST_ASSERT_TRUE(retomada_gravar(&nav));
        uint32_t durou = agora_us() - inicio;
        if (durou > pior_us) {
            pior_us = durou;
        }
    }

    TEST_ASSERT_LESS_THAN_UINT32(RETOMADA_LIMITE_US, pior_us);
    TEST_ASSERT_EQUAL_MEMORY(&antes, &nav, sizeof(nav));
}

static void test_so_grava_quando_a_celula_muda(void)
{
    nav_estado_t nav;
    retomada_sessao_t sessao;

    nav_iniciar(&nav, NAV_TIPO_4X4);
    andar(&nav, 2);
    retomada_sessao_iniciar(&sessao);

    TEST_ASSERT_TRUE(retomada_na_celula(&sessao, &nav));
    hal_nvs_sim_corromper("mapa", 40);
    /* Mesma célula: não regrava, então o CRC continua quebrado. */
    TEST_ASSERT_TRUE(retomada_na_celula(&sessao, &nav));
    TEST_ASSERT_FALSE(retomada_carregar(&nav, NAV_TIPO_4X4));

    nav_iniciar(&nav, NAV_TIPO_4X4);
    andar(&nav, 2);
    nav.x = (int8_t)(nav.x + 1);
    TEST_ASSERT_TRUE(retomada_na_celula(&sessao, &nav));
    TEST_ASSERT_TRUE(retomada_carregar(&nav, NAV_TIPO_4X4));
    TEST_ASSERT_EQUAL_INT8(sessao.x, nav.x);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_no_meio_do_trajeto_volta_a_mesma_celula);
    RUN_TEST(test_crc_errado_vira_mapa_vazio);
    RUN_TEST(test_crc_do_cabecalho_errado_vira_mapa_vazio);
    RUN_TEST(test_sem_registro_vira_mapa_vazio);
    RUN_TEST(test_tipo_diferente_do_dip_zera_o_mapa);
    RUN_TEST(test_gravacao_cabe_em_20_ms_e_nao_mexe_no_estado);
    RUN_TEST(test_so_grava_quando_a_celula_muda);
    return UNITY_END();
}
