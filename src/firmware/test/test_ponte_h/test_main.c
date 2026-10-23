/* Ponte H — FIRM-04 (#130) */

#include <unity.h>

#include "config/robo.h"
#include "esp32/ponte_h.h"
#include "hal_motor.h"

/* Com 6 V na ponte o duty não é escalado. */
#define SEIS_V MOTOR_TENSAO_NOMINAL_MV

void setUp(void) {}
void tearDown(void) {}

static void conferir(ponte_h_saida_t s, uint16_t duty, bool in1, bool in2)
{
    TEST_ASSERT_EQUAL_UINT16(duty, s.duty);
    TEST_ASSERT_EQUAL(in1, s.in1);
    TEST_ASSERT_EQUAL(in2, s.in2);
}

static void test_potencia_zero_para_com_as_duas_entradas_em_baixo(void)
{
    /* estado do boot */
    conferir(ponte_h_saida(0, false, SEIS_V), 0, false, false);
    conferir(ponte_h_saida(0, true, SEIS_V), 0, false, false);
}

static void test_frente_maxima(void)
{
    conferir(ponte_h_saida(MOTOR_POTENCIA_MAX, false, SEIS_V), PONTE_H_DUTY_MAX, true, false);
}

static void test_re_maxima(void)
{
    conferir(ponte_h_saida(-MOTOR_POTENCIA_MAX, false, SEIS_V), PONTE_H_DUTY_MAX, false, true);
}

static void test_duty_proporcional_a_potencia(void)
{
    conferir(ponte_h_saida(500, false, SEIS_V), 512, true, false); /* 1023 / 2 = 511,5 */
    conferir(ponte_h_saida(-250, false, SEIS_V), 256, false, true);
    conferir(ponte_h_saida(1, false, SEIS_V), 1, true, false);
}

static void test_fora_da_faixa_satura(void)
{
    conferir(ponte_h_saida(1500, false, SEIS_V), PONTE_H_DUTY_MAX, true, false);
    conferir(ponte_h_saida(-32768, false, SEIS_V), PONTE_H_DUTY_MAX, false, true);
}

static void test_invertido_troca_so_o_sentido(void)
{
    conferir(ponte_h_saida(400, true, SEIS_V), 409, false, true);
    conferir(ponte_h_saida(-400, true, SEIS_V), 409, true, false);
}

static void test_nunca_liga_as_duas_entradas(void)
{
    /* IN1 = IN2 = H trava o motor */
    for (int32_t p = -1100; p <= 1100; p += 50) {
        ponte_h_saida_t s = ponte_h_saida((int16_t)p, false, SEIS_V);
        TEST_ASSERT_FALSE(s.in1 && s.in2);
        TEST_ASSERT_TRUE(s.duty <= PONTE_H_DUTY_MAX);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_potencia_zero_para_com_as_duas_entradas_em_baixo);
    RUN_TEST(test_frente_maxima);
    RUN_TEST(test_re_maxima);
    RUN_TEST(test_duty_proporcional_a_potencia);
    RUN_TEST(test_fora_da_faixa_satura);
    RUN_TEST(test_invertido_troca_so_o_sentido);
    RUN_TEST(test_nunca_liga_as_duas_entradas);
    return UNITY_END();
}
