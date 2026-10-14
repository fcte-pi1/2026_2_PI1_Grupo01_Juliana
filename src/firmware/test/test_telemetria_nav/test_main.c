/*
 * Conversões da navegação para o contrato — FIRM-07 (#123): rumo N/L/S/O para
 * N/S/L/O e paredes relativas à frente para a máscara absoluta.
 */

#include <stdbool.h>
#include <unity.h>

#include "nav.h"
#include "telemetria_nav.h"

void setUp(void) {}
void tearDown(void) {}

static nav_leitura_t leitura(bool frente, bool esquerda, bool direita)
{
    nav_leitura_t l;
    l.frente = frente;
    l.esquerda = esquerda;
    l.direita = direita;
    return l;
}

static void test_rumo_nas_4_direcoes(void)
{
    TEST_ASSERT_EQUAL_INT(CONTRATO_RUMO_N, telemetria_rumo_do_nav(NAV_NORTE));
    TEST_ASSERT_EQUAL_INT(CONTRATO_RUMO_L, telemetria_rumo_do_nav(NAV_LESTE));
    TEST_ASSERT_EQUAL_INT(CONTRATO_RUMO_S, telemetria_rumo_do_nav(NAV_SUL));
    TEST_ASSERT_EQUAL_INT(CONTRATO_RUMO_O, telemetria_rumo_do_nav(NAV_OESTE));
}

static void test_parede_a_frente_e_a_esquerda_em_cada_rumo(void)
{
    nav_leitura_t l = leitura(true, true, false);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N | CONTRATO_PAREDE_O,
                            telemetria_paredes_do_nav(NAV_NORTE, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L | CONTRATO_PAREDE_N,
                            telemetria_paredes_do_nav(NAV_LESTE, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_S | CONTRATO_PAREDE_L,
                            telemetria_paredes_do_nav(NAV_SUL, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_O | CONTRATO_PAREDE_S,
                            telemetria_paredes_do_nav(NAV_OESTE, l));
}

static void test_parede_a_direita_em_cada_rumo(void)
{
    nav_leitura_t l = leitura(false, false, true);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L, telemetria_paredes_do_nav(NAV_NORTE, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_S, telemetria_paredes_do_nav(NAV_LESTE, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_O, telemetria_paredes_do_nav(NAV_SUL, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N, telemetria_paredes_do_nav(NAV_OESTE, l));
}

static void test_sem_parede_e_corredor(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, telemetria_paredes_do_nav(NAV_SUL, leitura(false, false, false)));
    /* Corredor norte-sul: paredes a leste e a oeste. */
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L | CONTRATO_PAREDE_O,
                            telemetria_paredes_do_nav(NAV_SUL, leitura(false, true, true)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rumo_nas_4_direcoes);
    RUN_TEST(test_parede_a_frente_e_a_esquerda_em_cada_rumo);
    RUN_TEST(test_parede_a_direita_em_cada_rumo);
    RUN_TEST(test_sem_parede_e_corredor);
    return UNITY_END();
}
