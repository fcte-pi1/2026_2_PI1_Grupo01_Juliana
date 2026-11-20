#include <unity.h>

#include "sinal.h"

static sinal_t s;

void setUp(void) { sinal_iniciar(&s); }

void tearDown(void) {}

static bool led(maq_estado_t estado, uint32_t t) { return sinal_led(&s, estado, t); }

static void test_health_check_pisca_rapido_200_aceso_200_apagado(void)
{
    TEST_ASSERT_TRUE(led(MAQ_HEALTH_CHECK, 1000));
    TEST_ASSERT_TRUE(led(MAQ_HEALTH_CHECK, 1199));
    TEST_ASSERT_FALSE(led(MAQ_HEALTH_CHECK, 1200));
    TEST_ASSERT_FALSE(led(MAQ_HEALTH_CHECK, 1399));
    TEST_ASSERT_TRUE(led(MAQ_HEALTH_CHECK, 1400));
}

static void test_aguardando_fica_aceso_o_tempo_todo(void)
{
    for (uint32_t t = 0; t < 5000; t += 137) {
        TEST_ASSERT_TRUE(led(MAQ_AGUARDANDO, t));
    }
}

static void test_reprovado_pisca_lento_1_s_aceso_1_s_apagado(void)
{
    TEST_ASSERT_TRUE(led(MAQ_REPROVADO, 0));
    TEST_ASSERT_TRUE(led(MAQ_REPROVADO, 999));
    TEST_ASSERT_FALSE(led(MAQ_REPROVADO, 1000));
    TEST_ASSERT_FALSE(led(MAQ_REPROVADO, 1999));
    TEST_ASSERT_TRUE(led(MAQ_REPROVADO, 2000));
}

static void test_running_pisca_medio_500_aceso_500_apagado(void)
{
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, 0));
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, 499));
    TEST_ASSERT_FALSE(led(MAQ_RUNNING, 500));
    TEST_ASSERT_FALSE(led(MAQ_RUNNING, 999));
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, 1000));
}

static void test_failed_pisca_muito_rapido_100_aceso_100_apagado(void)
{
    TEST_ASSERT_TRUE(led(MAQ_FAILED, 0));
    TEST_ASSERT_TRUE(led(MAQ_FAILED, 99));
    TEST_ASSERT_FALSE(led(MAQ_FAILED, 100));
    TEST_ASSERT_FALSE(led(MAQ_FAILED, 199));
    TEST_ASSERT_TRUE(led(MAQ_FAILED, 200));
}

static void test_success_pisca_3_vezes_e_fica_aceso(void)
{
    /* Piscadas de 200 ms: aceso 100, apagado 100, três vezes, até 600 ms. */
    int acesos_seguidos_para_apagados = 0;
    bool anterior = led(MAQ_SUCCESS, 0);
    TEST_ASSERT_TRUE(anterior);
    for (uint32_t t = 1; t < 600; t++) {
        bool agora = led(MAQ_SUCCESS, t);
        if (anterior && !agora) {
            acesos_seguidos_para_apagados++;
        }
        anterior = agora;
    }
    TEST_ASSERT_EQUAL_INT(3, acesos_seguidos_para_apagados);
    for (uint32_t t = 600; t < 10000; t += 97) {
        TEST_ASSERT_TRUE(led(MAQ_SUCCESS, t));
    }
}

static void test_mudar_de_estado_reinicia_o_padrao(void)
{
    led(MAQ_RUNNING, 0);
    TEST_ASSERT_FALSE(led(MAQ_RUNNING, 700)); /* na fase apagada */
    TEST_ASSERT_TRUE(led(MAQ_FAILED, 700));   /* o novo estado começa aceso */
    TEST_ASSERT_FALSE(led(MAQ_FAILED, 800));
}

static void test_estado_repetido_nao_reinicia_o_padrao(void)
{
    led(MAQ_REPROVADO, 5000);
    TEST_ASSERT_FALSE(led(MAQ_REPROVADO, 6500));
    TEST_ASSERT_FALSE(led(MAQ_REPROVADO, 6600));
}

static void test_a_virada_do_relogio_de_32_bits_nao_quebra_o_padrao(void)
{
    uint32_t inicio = 0xFFFFFF00u;
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, inicio));
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, inicio + 255));
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, 100));  /* 356 ms desde o início, já depois da virada */
    TEST_ASSERT_FALSE(led(MAQ_RUNNING, 300)); /* 556 ms: fase apagada */
    TEST_ASSERT_TRUE(led(MAQ_RUNNING, 744));  /* 1000 ms: começa o período seguinte */
}

#ifdef ARDUINO
/* Na ESP32 o ponto de entrada é o setup() de entrada_arduino.cpp. */
int executar_testes(void)
#else
int main(void)
#endif
{
    UNITY_BEGIN();
    RUN_TEST(test_health_check_pisca_rapido_200_aceso_200_apagado);
    RUN_TEST(test_aguardando_fica_aceso_o_tempo_todo);
    RUN_TEST(test_reprovado_pisca_lento_1_s_aceso_1_s_apagado);
    RUN_TEST(test_running_pisca_medio_500_aceso_500_apagado);
    RUN_TEST(test_failed_pisca_muito_rapido_100_aceso_100_apagado);
    RUN_TEST(test_success_pisca_3_vezes_e_fica_aceso);
    RUN_TEST(test_mudar_de_estado_reinicia_o_padrao);
    RUN_TEST(test_estado_repetido_nao_reinicia_o_padrao);
    RUN_TEST(test_a_virada_do_relogio_de_32_bits_nao_quebra_o_padrao);
    return UNITY_END();
}
