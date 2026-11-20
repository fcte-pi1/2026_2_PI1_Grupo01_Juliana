#include <unity.h>

#include "esp32/encoder_soma.h"

#define LIMITE 30000

static encoder_soma_t s;

void setUp(void) { encoder_soma_iniciar(&s, 0); }

void tearDown(void) {}

/* O hardware conta de 1 em 1 e zera ao chegar no limite. */
static int32_t contar(int passos, int16_t *contador)
{
    int32_t total = 0;
    for (int i = 0; i < passos; i++) {
        (*contador)++;
        if (*contador >= LIMITE) {
            *contador = 0;
        }
        total = encoder_soma_atualizar(&s, *contador, LIMITE);
    }
    return total;
}

static void test_sem_voltar_a_zero_soma_a_contagem(void)
{
    TEST_ASSERT_EQUAL_INT32(0, encoder_soma_atualizar(&s, 0, LIMITE));
    TEST_ASSERT_EQUAL_INT32(840, encoder_soma_atualizar(&s, 840, LIMITE));
    TEST_ASSERT_EQUAL_INT32(-120, encoder_soma_atualizar(&s, -120, LIMITE));
}

static void test_passar_do_limite_para_a_frente_nao_perde_pulsos(void)
{
    int16_t contador = 0;
    TEST_ASSERT_EQUAL_INT32(LIMITE - 3, contar(LIMITE - 3, &contador));
    TEST_ASSERT_EQUAL_INT32(LIMITE + 7, contar(10, &contador));
}

static void test_varias_voltas_do_contador_para_a_frente(void)
{
    int16_t contador = 0;
    TEST_ASSERT_EQUAL_INT32(5 * LIMITE + 123, contar(5 * LIMITE + 123, &contador));
}

static void test_passar_do_limite_para_tras_nao_perde_pulsos(void)
{
    encoder_soma_iniciar(&s, 0);
    int16_t contador = 0;
    int32_t total = 0;
    for (int i = 0; i < LIMITE + 20; i++) {
        contador--;
        if (contador <= -LIMITE) {
            contador = 0;
        }
        total = encoder_soma_atualizar(&s, contador, LIMITE);
    }
    TEST_ASSERT_EQUAL_INT32(-(LIMITE + 20), total);
}

static void test_ir_e_voltar_perto_do_limite_mantem_o_total(void)
{
    int16_t contador = 0;
    contar(LIMITE - 2, &contador);
    TEST_ASSERT_EQUAL_INT32(LIMITE + 1, contar(3, &contador)); /* zerou */
    contador--;                                                 /* volta 1 pulso */
    TEST_ASSERT_EQUAL_INT32(LIMITE, encoder_soma_atualizar(&s, contador, LIMITE));
}

static void test_iniciar_com_o_contador_do_hardware_diferente_de_zero(void)
{
    encoder_soma_iniciar(&s, 500);
    TEST_ASSERT_EQUAL_INT32(0, encoder_soma_atualizar(&s, 500, LIMITE));
    TEST_ASSERT_EQUAL_INT32(40, encoder_soma_atualizar(&s, 540, LIMITE));
}

#ifdef ARDUINO
/* Na ESP32 o ponto de entrada é o setup() de entrada_arduino.cpp. */
int executar_testes(void)
#else
int main(void)
#endif
{
    UNITY_BEGIN();
    RUN_TEST(test_sem_voltar_a_zero_soma_a_contagem);
    RUN_TEST(test_passar_do_limite_para_a_frente_nao_perde_pulsos);
    RUN_TEST(test_varias_voltas_do_contador_para_a_frente);
    RUN_TEST(test_passar_do_limite_para_tras_nao_perde_pulsos);
    RUN_TEST(test_ir_e_voltar_perto_do_limite_mantem_o_total);
    RUN_TEST(test_iniciar_com_o_contador_do_hardware_diferente_de_zero);
    return UNITY_END();
}
