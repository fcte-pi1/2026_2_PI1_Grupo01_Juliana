/*
 * Estes testes compilam no PC, onde não existe Arduino.h: se alguém incluir
 * Arduino na pasta navegacao/, este ambiente quebra. Os testes do flood fill
 * entram aqui com o FIRM-01 (#117).
 */

#include <unity.h>

#include "nav.h"

void setUp(void) {}
void tearDown(void) {}

static void conferir_largada(nav_tipo_t tipo)
{
    nav_estado_t nav;
    nav_iniciar(&nav, tipo);
    TEST_ASSERT_EQUAL_INT8(0, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav.direcao);
    TEST_ASSERT_EQUAL_INT(tipo, nav.tipo);
    TEST_ASSERT_EQUAL_INT(NAV_LADO_INDEFINIDO, nav.lado);
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_INDEFINIDA, nav_orientacao(&nav));
    TEST_ASSERT_FALSE(nav.terminou);
}

static void test_largada_no_4x4(void)
{
    conferir_largada(NAV_TIPO_4X4);
}

static void test_largada_no_8x4(void)
{
    conferir_largada(NAV_TIPO_8X4);
}

static void test_largada_no_12x4(void)
{
    conferir_largada(NAV_TIPO_12X4);
}

static void test_distancia_fora_da_matriz_e_infinita(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, -1, 0));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 0, -1));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, NAV_MAX_CELULAS, 0));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 0, NAV_MAX_CELULAS));
}

static void test_direcao_depois_de_cada_acao(void)
{
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav_direcao_apos(NAV_NORTE, NAV_ACAO_DIREITA));
    TEST_ASSERT_EQUAL_INT(NAV_OESTE, nav_direcao_apos(NAV_NORTE, NAV_ACAO_ESQUERDA));
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav_direcao_apos(NAV_NORTE, NAV_ACAO_MEIA_VOLTA));
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav_direcao_apos(NAV_OESTE, NAV_ACAO_DIREITA));
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav_direcao_apos(NAV_SUL, NAV_ACAO_FRENTE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_largada_no_4x4);
    RUN_TEST(test_largada_no_8x4);
    RUN_TEST(test_largada_no_12x4);
    RUN_TEST(test_distancia_fora_da_matriz_e_infinita);
    RUN_TEST(test_direcao_depois_de_cada_acao);
    return UNITY_END();
}
