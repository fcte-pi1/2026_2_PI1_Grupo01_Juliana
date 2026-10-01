/*
 * Estes testes compilam no PC, onde não existe Arduino.h: se alguém incluir
 * Arduino na pasta navegacao/, este ambiente quebra. Os testes do flood fill
 * entram aqui com o FIRM-01 (#117).
 */

#include <unity.h>

#include "nav.h"

void setUp(void) {}
void tearDown(void) {}

static void test_largada_em_0_0_virado_para_o_norte(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav);
    TEST_ASSERT_EQUAL_INT8(0, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav.direcao);
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
    RUN_TEST(test_largada_em_0_0_virado_para_o_norte);
    RUN_TEST(test_direcao_depois_de_cada_acao);
    return UNITY_END();
}
