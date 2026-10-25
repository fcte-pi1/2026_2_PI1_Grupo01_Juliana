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
                            telemetria_paredes_do_nav(NAV_NORTE, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L | CONTRATO_PAREDE_N,
                            telemetria_paredes_do_nav(NAV_LESTE, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_S | CONTRATO_PAREDE_L,
                            telemetria_paredes_do_nav(NAV_SUL, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_O | CONTRATO_PAREDE_S,
                            telemetria_paredes_do_nav(NAV_OESTE, NAV_LADO_DIREITA, l));
}

static void test_parede_a_direita_em_cada_rumo(void)
{
    nav_leitura_t l = leitura(false, false, true);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L, telemetria_paredes_do_nav(NAV_NORTE, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_S, telemetria_paredes_do_nav(NAV_LESTE, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_O, telemetria_paredes_do_nav(NAV_SUL, NAV_LADO_DIREITA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N, telemetria_paredes_do_nav(NAV_OESTE, NAV_LADO_DIREITA, l));
}

static void test_sem_parede_e_corredor(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, telemetria_paredes_do_nav(NAV_SUL, NAV_LADO_DIREITA, leitura(false, false, false)));
    /* Corredor norte-sul: paredes a leste e a oeste. */
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L | CONTRATO_PAREDE_O,
                            telemetria_paredes_do_nav(NAV_SUL, NAV_LADO_DIREITA, leitura(false, true, true)));
}

static void test_lado_esquerda_troca_as_laterais(void)
{
    nav_leitura_t l = leitura(true, false, true);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N | CONTRATO_PAREDE_O,
                            telemetria_paredes_do_nav(NAV_NORTE, NAV_LADO_ESQUERDA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L | CONTRATO_PAREDE_N,
                            telemetria_paredes_do_nav(NAV_LESTE, NAV_LADO_ESQUERDA, l));
    l = leitura(false, true, false);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_L,
                            telemetria_paredes_do_nav(NAV_NORTE, NAV_LADO_ESQUERDA, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N,
                            telemetria_paredes_do_nav(NAV_OESTE, NAV_LADO_ESQUERDA, l));
}

static void test_lado_indefinido_vale_como_direita(void)
{
    nav_leitura_t l = leitura(true, false, true);
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_N | CONTRATO_PAREDE_L,
                            telemetria_paredes_do_nav(NAV_NORTE, NAV_LADO_INDEFINIDO, l));
    TEST_ASSERT_EQUAL_UINT8(CONTRATO_PAREDE_S | CONTRATO_PAREDE_O,
                            telemetria_paredes_do_nav(NAV_SUL, NAV_LADO_INDEFINIDO, l));
}

static telemetria_decisao_t decisao(bool mudou, bool terminou, nav_acao_t acao)
{
    telemetria_decisao_t d = {0};
    d.mudou_celula = mudou;
    d.x = 2;
    d.y = 3;
    d.paredes = 1;
    d.terminou_agora = terminou;
    d.acao = acao;
    d.nav_x = 3;
    d.nav_y = 3;
    return d;
}

static void test_passo_sai_antes_do_sucesso(void)
{
    telemetria_decisao_t d = decisao(true, true, NAV_ACAO_SUCESSO);
    telemetria_decisao_evento_t out[TELEMETRIA_DECISAO_EVENTOS_MAX];
    TEST_ASSERT_EQUAL_INT(2, telemetria_eventos_da_decisao(&d, out));
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_PASSO, out[0].tipo);
    TEST_ASSERT_EQUAL_UINT8(2, out[0].x);
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_SUCESSO, out[1].tipo);
    TEST_ASSERT_EQUAL_UINT8(3, out[1].x);
}

static void test_passo_sai_antes_da_falha_sem_saida(void)
{
    telemetria_decisao_t d = decisao(true, true, NAV_ACAO_SEM_PASSO);
    telemetria_decisao_evento_t out[TELEMETRIA_DECISAO_EVENTOS_MAX];
    TEST_ASSERT_EQUAL_INT(2, telemetria_eventos_da_decisao(&d, out));
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_PASSO, out[0].tipo);
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_SEM_SAIDA, out[1].tipo);
}

static void test_decisao_sem_novidade_nao_publica(void)
{
    telemetria_decisao_t d = decisao(false, false, NAV_ACAO_FRENTE);
    telemetria_decisao_evento_t out[TELEMETRIA_DECISAO_EVENTOS_MAX];
    TEST_ASSERT_EQUAL_INT(0, telemetria_eventos_da_decisao(&d, out));
}

static void test_fim_na_mesma_celula_publica_so_o_sucesso(void)
{
    telemetria_decisao_t d = decisao(false, true, NAV_ACAO_SUCESSO);
    telemetria_decisao_evento_t out[TELEMETRIA_DECISAO_EVENTOS_MAX];
    TEST_ASSERT_EQUAL_INT(1, telemetria_eventos_da_decisao(&d, out));
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_SUCESSO, out[0].tipo);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rumo_nas_4_direcoes);
    RUN_TEST(test_parede_a_frente_e_a_esquerda_em_cada_rumo);
    RUN_TEST(test_parede_a_direita_em_cada_rumo);
    RUN_TEST(test_sem_parede_e_corredor);
    RUN_TEST(test_lado_esquerda_troca_as_laterais);
    RUN_TEST(test_lado_indefinido_vale_como_direita);
    RUN_TEST(test_passo_sai_antes_do_sucesso);
    RUN_TEST(test_passo_sai_antes_da_falha_sem_saida);
    RUN_TEST(test_decisao_sem_novidade_nao_publica);
    RUN_TEST(test_fim_na_mesma_celula_publica_so_o_sucesso);
    return UNITY_END();
}
