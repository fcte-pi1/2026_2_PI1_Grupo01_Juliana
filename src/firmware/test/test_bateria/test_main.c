/*
 * Leitura da bateria — FIRM-06 (#128). Critério de aceite: conversão e alerta
 * corretos com tensões simuladas (8,4 V, 7,4 V, 7,0 V e pilhas AA).
 */

#include <unity.h>

#include "bateria.h"

void setUp(void) {}
void tearDown(void) {}

/* O que o ADC lê com a fonte em mV: o divisor fica depois do diodo. */
static uint16_t adc_de(uint16_t fonte_mv)
{
    return (uint16_t)((fonte_mv - BATERIA_QUEDA_DIODO_MV) / BATERIA_DIVISOR + 0.5f);
}

static void encher(bateria_t *b, uint16_t fonte_mv)
{
    for (int i = 0; i < BATERIA_MEDIA_AMOSTRAS; i++) {
        bateria_amostrar(b, adc_de(fonte_mv));
    }
}

static void conferir(uint16_t fonte_mv, bateria_fonte_t fonte, uint8_t pct, bateria_nivel_t nivel)
{
    bateria_t b;
    bateria_iniciar(&b);
    encher(&b, fonte_mv);

    TEST_ASSERT_TRUE(bateria_pronta(&b));
    TEST_ASSERT_UINT16_WITHIN(3, fonte_mv, bateria_mv(&b)); /* arredondamento do ADC em mV */
    TEST_ASSERT_EQUAL_INT(fonte, bateria_fonte(&b));
    TEST_ASSERT_UINT8_WITHIN(1, pct, bateria_percentual(&b));
    TEST_ASSERT_EQUAL_INT(nivel, bateria_nivel(&b));
}

static void test_lipo_cheia(void) { conferir(8400, BATERIA_FONTE_LIPO, 100, BATERIA_NORMAL); }

static void test_lipo_no_alerta(void) { conferir(7400, BATERIA_FONTE_LIPO, 15, BATERIA_ALERTA); }

static void test_lipo_no_corte_ainda_e_alerta(void)
{
    conferir(7010, BATERIA_FONTE_LIPO, 1, BATERIA_ALERTA);
}

static void test_lipo_abaixo_do_corte_e_low_battery(void)
{
    conferir(6900, BATERIA_FONTE_LIPO, 0, BATERIA_BAIXA);
}

static void test_lipo_que_cai_abaixo_do_limiar_continua_lipo(void)
{
    bateria_t b;
    bateria_iniciar(&b);
    encher(&b, 7600);
    encher(&b, 6500); /* sem histórico pareceria pilha; a fonte foi fixada na primeira janela */
    TEST_ASSERT_EQUAL_INT(BATERIA_FONTE_LIPO, bateria_fonte(&b));
    TEST_ASSERT_EQUAL_UINT8(0, bateria_percentual(&b));
    TEST_ASSERT_EQUAL_INT(BATERIA_BAIXA, bateria_nivel(&b));
}

static void test_pilhas_novas(void) { conferir(6000, BATERIA_FONTE_AA, 80, BATERIA_NORMAL); }

/* Nas pilhas, alerta e corte são os dois 5,0 V [ENE]: o alerta é só o próprio limite (ver test_limites_exatos). */
static void test_pilhas_perto_do_alerta(void) { conferir(5200, BATERIA_FONTE_AA, 20, BATERIA_NORMAL); }

static void test_pilhas_abaixo_de_5v_e_low_battery(void)
{
    conferir(4900, BATERIA_FONTE_AA, 0, BATERIA_BAIXA);
}

static void test_limites_exatos(void)
{
    TEST_ASSERT_EQUAL_INT(BATERIA_NORMAL, bateria_nivel_de(BATERIA_FONTE_LIPO, 7401));
    TEST_ASSERT_EQUAL_INT(BATERIA_ALERTA, bateria_nivel_de(BATERIA_FONTE_LIPO, 7400));
    TEST_ASSERT_EQUAL_INT(BATERIA_ALERTA, bateria_nivel_de(BATERIA_FONTE_LIPO, 7000));
    TEST_ASSERT_EQUAL_INT(BATERIA_BAIXA, bateria_nivel_de(BATERIA_FONTE_LIPO, 6999));
    TEST_ASSERT_EQUAL_INT(BATERIA_NORMAL, bateria_nivel_de(BATERIA_FONTE_AA, 5001));
    TEST_ASSERT_EQUAL_INT(BATERIA_ALERTA, bateria_nivel_de(BATERIA_FONTE_AA, 5000));
    TEST_ASSERT_EQUAL_INT(BATERIA_BAIXA, bateria_nivel_de(BATERIA_FONTE_AA, 4999));
}

static void test_percentual_interpola_e_satura(void)
{
    TEST_ASSERT_EQUAL_UINT8(100, bateria_percentual_de(BATERIA_FONTE_LIPO, 8600));
    TEST_ASSERT_EQUAL_UINT8(65, bateria_percentual_de(BATERIA_FONTE_LIPO, 7900)); /* entre 75 e 55 */
    TEST_ASSERT_EQUAL_UINT8(0, bateria_percentual_de(BATERIA_FONTE_LIPO, 6500));
    TEST_ASSERT_EQUAL_UINT8(65, bateria_percentual_de(BATERIA_FONTE_AA, 5800)); /* entre 80 e 50 */
    TEST_ASSERT_EQUAL_UINT8(100, bateria_percentual_de(BATERIA_FONTE_AA, 6600));
}

static void test_media_movel_descarta_as_leituras_antigas(void)
{
    bateria_t b;
    bateria_iniciar(&b);
    TEST_ASSERT_EQUAL_UINT16(0, bateria_mv(&b));

    encher(&b, 8000);
    bateria_amostrar(&b, adc_de(7200)); /* um pico de corrente não derruba a leitura */
    TEST_ASSERT_TRUE(bateria_mv(&b) > 7900);

    encher(&b, 7600);
    TEST_ASSERT_UINT16_WITHIN(3, 7600, bateria_mv(&b));
}

static void test_media_parcial_antes_da_janela_cheia(void)
{
    bateria_t b;
    bateria_iniciar(&b);
    bateria_amostrar(&b, adc_de(8000));
    bateria_amostrar(&b, adc_de(7800));

    TEST_ASSERT_FALSE(bateria_pronta(&b));
    TEST_ASSERT_UINT16_WITHIN(3, 7900, bateria_mv(&b));
    TEST_ASSERT_EQUAL_INT(BATERIA_FONTE_LIPO, bateria_fonte(&b));
}

static void test_ponte_nao_soma_o_diodo(void)
{
    bateria_t b;
    bateria_iniciar(&b);
    encher(&b, 8400);

    TEST_ASSERT_EQUAL_UINT16(bateria_mv(&b) - BATERIA_QUEDA_DIODO_MV, bateria_ponte_mv(&b));
    TEST_ASSERT_UINT16_WITHIN(3, 2516, adc_de(8400)); /* 2,52 V no pino, tabela 10 do 4.2 */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lipo_cheia);
    RUN_TEST(test_lipo_no_alerta);
    RUN_TEST(test_lipo_no_corte_ainda_e_alerta);
    RUN_TEST(test_lipo_abaixo_do_corte_e_low_battery);
    RUN_TEST(test_lipo_que_cai_abaixo_do_limiar_continua_lipo);
    RUN_TEST(test_pilhas_novas);
    RUN_TEST(test_pilhas_perto_do_alerta);
    RUN_TEST(test_pilhas_abaixo_de_5v_e_low_battery);
    RUN_TEST(test_limites_exatos);
    RUN_TEST(test_percentual_interpola_e_satura);
    RUN_TEST(test_media_movel_descarta_as_leituras_antigas);
    RUN_TEST(test_media_parcial_antes_da_janela_cheia);
    RUN_TEST(test_ponte_nao_soma_o_diodo);
    return UNITY_END();
}
