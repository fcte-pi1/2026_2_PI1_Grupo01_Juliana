/* Gestos do botão BOOT: debounce, toque na corrida, toque curto e toque longo. */

#include <unity.h>

#include "botao.h"
#include "config/robo.h"
#include "hal_boot.h"
#include "sim_mundo.h"

static botao_t b;

void setUp(void)
{
    botao_iniciar(&b);
    sim_mundo_iniciar("+---+\n|   |\n+---+\n");
}

void tearDown(void) {}

/* Passa o tempo de 1 em 1 ms com o botão no nível dado; devolve o primeiro gesto que sair. */
static botao_gesto_t segurar(bool nivel, uint32_t de_ms, uint32_t ate_ms, bool em_corrida)
{
    botao_gesto_t primeiro = BOTAO_NADA;
    for (uint32_t t = de_ms; t < ate_ms; t++) {
        botao_gesto_t g = botao_passo(&b, nivel, t, em_corrida);
        if (g != BOTAO_NADA && primeiro == BOTAO_NADA) {
            primeiro = g;
        }
    }
    return primeiro;
}

void test_sem_apertar_nao_sai_nada(void)
{
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 0, 5000, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 5000, 8000, true));
}

void test_toque_curto_sai_ao_soltar(void)
{
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 400, false));
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 400, 500, false));
}

void test_toque_curto_so_sai_uma_vez(void)
{
    segurar(true, 100, 400, false);
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 400, 500, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 500, 3000, false));
}

void test_um_milissegundo_antes_do_limite_ainda_e_toque_curto(void)
{
    /* Apertou estável em 100 ms (o aperto conta desde a mudança): solta antes de 100 + LONGO. */
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 100 + BOTAO_LONGO_MS - 1, false));
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 100 + BOTAO_LONGO_MS - 1, 100 + BOTAO_LONGO_MS + 100, false));
}

void test_toque_longo_sai_com_o_botao_ainda_apertado(void)
{
    botao_gesto_t g = segurar(true, 100, 100 + BOTAO_LONGO_MS + 50, false);
    TEST_ASSERT_EQUAL(BOTAO_LONGO, g);
}

void test_toque_longo_sai_no_instante_do_limite(void)
{
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 100 + BOTAO_LONGO_MS, false));
    TEST_ASSERT_EQUAL(BOTAO_LONGO, botao_passo(&b, true, 100 + BOTAO_LONGO_MS, false));
}

void test_soltar_depois_do_toque_longo_nao_gera_toque_curto(void)
{
    segurar(true, 100, 100 + BOTAO_LONGO_MS + 200, false);
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 100 + BOTAO_LONGO_MS + 200, 4000, false));
}

void test_toque_longo_sai_uma_vez_so_mesmo_segurando_mais(void)
{
    TEST_ASSERT_EQUAL(BOTAO_LONGO, segurar(true, 100, 5000, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 5000, 9000, false));
}

void test_em_corrida_o_toque_sai_ao_apertar(void)
{
    TEST_ASSERT_EQUAL(BOTAO_TOQUE, segurar(true, 100, 200, true));
}

void test_em_corrida_soltar_ou_segurar_nao_gera_mais_nada(void)
{
    TEST_ASSERT_EQUAL(BOTAO_TOQUE, segurar(true, 100, 200, true));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 200, 4000, true));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 4000, 4300, true));
}

void test_aperto_durante_a_corrida_solto_depois_dela_nao_vira_curto(void)
{
    TEST_ASSERT_EQUAL(BOTAO_TOQUE, segurar(true, 100, 200, true));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 200, 400, false));
}

void test_aperto_antes_da_corrida_vira_toque_quando_a_corrida_comeca_so_no_proximo_aperto(void)
{
    /* Apertado parado e ainda segurado quando a corrida começa: não vira toque, nem longo. */
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 600, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 600, 4000, true));
}

void test_ruido_curto_e_ignorado(void)
{
    /* Contato que oscila em intervalos menores que o debounce nunca estabiliza. */
    botao_gesto_t gesto = BOTAO_NADA;
    for (uint32_t t = 100; t < 200; t += 10) {
        botao_gesto_t g = botao_passo(&b, (t / 10) % 2 == 0, t, false);
        if (g != BOTAO_NADA) {
            gesto = g;
        }
    }
    TEST_ASSERT_EQUAL(BOTAO_NADA, gesto);
}

void test_pulso_menor_que_o_debounce_e_ignorado(void)
{
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 100 + BOTAO_DEBOUNCE_MS - 1, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(false, 100 + BOTAO_DEBOUNCE_MS - 1, 1000, false));
}

void test_aperto_com_ruido_no_inicio_ainda_conta_como_um_toque(void)
{
    /* Quica por 10 ms e fica apertado: um único toque curto ao soltar. */
    botao_passo(&b, true, 100, false);
    botao_passo(&b, false, 105, false);
    botao_passo(&b, true, 108, false);
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 109, 400, false));
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 400, 500, false));
}

void test_dois_toques_curtos_seguidos_dao_dois_gestos(void)
{
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 100, 300, false));
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 300, 500, false));
    TEST_ASSERT_EQUAL(BOTAO_NADA, segurar(true, 500, 700, false));
    TEST_ASSERT_EQUAL(BOTAO_CURTO, segurar(false, 700, 900, false));
}

void test_funciona_na_virada_do_contador_de_32_bits(void)
{
    uint32_t inicio = 0xFFFFFF00u;
    botao_gesto_t g = BOTAO_NADA;
    for (uint32_t n = 0; n < BOTAO_LONGO_MS + 200u; n++) {
        botao_gesto_t x = botao_passo(&b, true, inicio + n, false); /* passa de 2^32 e volta a 0 */
        if (x != BOTAO_NADA && g == BOTAO_NADA) {
            g = x;
        }
    }
    TEST_ASSERT_EQUAL(BOTAO_LONGO, g);
}

/* ---------- com a HAL simulada: o botão do mundo simulado e o relógio dele ---------- */

static botao_gesto_t avancar_sim(uint32_t ms, bool em_corrida)
{
    botao_gesto_t primeiro = BOTAO_NADA;
    for (uint32_t i = 0; i < ms; i++) {
        sim_mundo_avancar(1);
        botao_gesto_t g = botao_passo(&b, hal_boot_pressionado(), sim_tempo_ms(), em_corrida);
        if (g != BOTAO_NADA && primeiro == BOTAO_NADA) {
            primeiro = g;
        }
    }
    return primeiro;
}

void test_botao_da_hal_simulada_da_toque_curto(void)
{
    avancar_sim(100, false);
    sim_definir_boot(true);
    TEST_ASSERT_EQUAL(BOTAO_NADA, avancar_sim(300, false));
    sim_definir_boot(false);
    TEST_ASSERT_EQUAL(BOTAO_CURTO, avancar_sim(100, false));
}

void test_botao_da_hal_simulada_da_toque_longo(void)
{
    avancar_sim(100, false);
    sim_definir_boot(true);
    TEST_ASSERT_EQUAL(BOTAO_LONGO, avancar_sim(BOTAO_LONGO_MS + 100, false));
}

void test_botao_da_hal_simulada_para_a_corrida(void)
{
    avancar_sim(100, true);
    sim_definir_boot(true);
    TEST_ASSERT_EQUAL(BOTAO_TOQUE, avancar_sim(100, true));
}

#ifdef ARDUINO
/* Na ESP32 o ponto de entrada é o setup() de entrada_arduino.cpp. */
int executar_testes(void)
#else
int main(void)
#endif
{
    UNITY_BEGIN();
    RUN_TEST(test_sem_apertar_nao_sai_nada);
    RUN_TEST(test_toque_curto_sai_ao_soltar);
    RUN_TEST(test_toque_curto_so_sai_uma_vez);
    RUN_TEST(test_um_milissegundo_antes_do_limite_ainda_e_toque_curto);
    RUN_TEST(test_toque_longo_sai_com_o_botao_ainda_apertado);
    RUN_TEST(test_toque_longo_sai_no_instante_do_limite);
    RUN_TEST(test_soltar_depois_do_toque_longo_nao_gera_toque_curto);
    RUN_TEST(test_toque_longo_sai_uma_vez_so_mesmo_segurando_mais);
    RUN_TEST(test_em_corrida_o_toque_sai_ao_apertar);
    RUN_TEST(test_em_corrida_soltar_ou_segurar_nao_gera_mais_nada);
    RUN_TEST(test_aperto_durante_a_corrida_solto_depois_dela_nao_vira_curto);
    RUN_TEST(test_aperto_antes_da_corrida_vira_toque_quando_a_corrida_comeca_so_no_proximo_aperto);
    RUN_TEST(test_ruido_curto_e_ignorado);
    RUN_TEST(test_pulso_menor_que_o_debounce_e_ignorado);
    RUN_TEST(test_aperto_com_ruido_no_inicio_ainda_conta_como_um_toque);
    RUN_TEST(test_dois_toques_curtos_seguidos_dao_dois_gestos);
    RUN_TEST(test_funciona_na_virada_do_contador_de_32_bits);
    RUN_TEST(test_botao_da_hal_simulada_da_toque_curto);
    RUN_TEST(test_botao_da_hal_simulada_da_toque_longo);
    RUN_TEST(test_botao_da_hal_simulada_para_a_corrida);
    return UNITY_END();
}
