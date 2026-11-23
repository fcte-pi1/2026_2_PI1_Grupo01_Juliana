/* Tipo do labirinto pelo DIP switch. */

#include <unity.h>

#include "dip.h"

void setUp(void) {}
void tearDown(void) {}

void test_codigos_das_vias_1_e_2(void)
{
    TEST_ASSERT_EQUAL(CONTRATO_DIP_4X4, dip_tipo(0x0));   /* 00 */
    TEST_ASSERT_EQUAL(CONTRATO_DIP_8X4, dip_tipo(0x1));   /* via 1 ligada */
    TEST_ASSERT_EQUAL(CONTRATO_DIP_12X4, dip_tipo(0x2));  /* via 2 ligada */
    TEST_ASSERT_EQUAL(CONTRATO_DIP_INVALIDO, dip_tipo(0x3)); /* 11 */
}

void test_vias_3_e_4_nao_mudam_o_tipo(void)
{
    for (uint8_t altas = 0; altas < 4; altas++) {
        for (uint8_t baixas = 0; baixas < 4; baixas++) {
            TEST_ASSERT_EQUAL(dip_tipo(baixas), dip_tipo((uint8_t)((altas << 2) | baixas)));
        }
    }
}

void test_tipo_da_navegacao_acompanha_o_do_contrato(void)
{
    TEST_ASSERT_EQUAL(NAV_TIPO_4X4, dip_tipo_nav(0x0));
    TEST_ASSERT_EQUAL(NAV_TIPO_8X4, dip_tipo_nav(0x1));
    TEST_ASSERT_EQUAL(NAV_TIPO_12X4, dip_tipo_nav(0x2));
}

void test_codigo_invalido_cai_em_4x4_na_navegacao(void)
{
    TEST_ASSERT_EQUAL(NAV_TIPO_4X4, dip_tipo_nav(0x3));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_codigos_das_vias_1_e_2);
    RUN_TEST(test_vias_3_e_4_nao_mudam_o_tipo);
    RUN_TEST(test_tipo_da_navegacao_acompanha_o_do_contrato);
    RUN_TEST(test_codigo_invalido_cai_em_4x4_na_navegacao);
    return UNITY_END();
}
