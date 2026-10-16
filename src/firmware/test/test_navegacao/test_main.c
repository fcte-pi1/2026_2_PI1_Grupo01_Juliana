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

static nav_leitura_t leitura(bool frente, bool esquerda, bool direita)
{
    nav_leitura_t l = {frente, esquerda, direita};
    return l;
}

static bool tem_parede(const nav_estado_t *nav, int x, int y, uint8_t parede)
{
    return (nav->mapa[x][y].paredes & parede) != 0;
}

/* Põe o robô numa célula e rumo quaisquer, como se já tivesse andado até lá. */
static void posicionar(nav_estado_t *nav, int x, int y, nav_direcao_t direcao)
{
    nav->x = (int8_t)x;
    nav->y = (int8_t)y;
    nav->direcao = direcao;
}

static void test_largada_grava_as_bordas_sul_e_oeste(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    for (int i = 0; i < NAV_MAX_CELULAS; i++) {
        TEST_ASSERT_TRUE(tem_parede(&nav, i, 0, NAV_PAREDE_S));
        TEST_ASSERT_TRUE(tem_parede(&nav, 0, i, NAV_PAREDE_O));
    }
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_S));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
}

static void test_parede_gravada_nos_dois_lados(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav.lado = NAV_LADO_DIREITA;
    nav.mapa_alterado = false;
    nav_passo(&nav, leitura(true, true, true));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 2, NAV_PAREDE_S));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
    TEST_ASSERT_TRUE(nav.mapa_alterado);
}

static void test_leitura_com_rumo_leste_gira_as_paredes(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 1, 1, NAV_LESTE);
    nav.lado = NAV_LADO_DIREITA;
    nav_passo(&nav, leitura(true, true, false));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 2, 1, NAV_PAREDE_O));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_S));
}

static void test_lado_esquerda_espelha_as_leituras_laterais(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.lado = NAV_LADO_ESQUERDA;
    nav_passo(&nav, leitura(false, true, false));
    /* Com o x positivo à esquerda do robô, a esquerda física é o leste. */
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 2, 1, NAV_PAREDE_O));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
}

static void test_leitura_aberta_nao_apaga_parede(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.lado = NAV_LADO_DIREITA;
    nav_passo(&nav, leitura(true, true, true));
    nav.mapa_alterado = false;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
    TEST_ASSERT_FALSE(nav.mapa_alterado);
}

static void test_parede_ja_conhecida_nao_liga_a_flag(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.lado = NAV_LADO_DIREITA;
    nav_passo(&nav, leitura(true, false, false));
    nav.mapa_alterado = false;
    nav_passo(&nav, leitura(true, false, false));
    TEST_ASSERT_FALSE(nav.mapa_alterado);
}

static void test_so_a_direita_aberta_fixa_o_lado_direita(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, true, false));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
    TEST_ASSERT_FALSE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
}

static void test_so_a_esquerda_aberta_fixa_o_lado_esquerda(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, true));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_ESQUERDA, nav.lado);
    /* A abertura vira o leste, e a parede da direita cai na borda oeste. */
    TEST_ASSERT_FALSE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_O));
}

static void test_duas_laterais_abertas_fixam_o_lado_direita(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
}

static void test_duas_laterais_fechadas_gravam_o_leste(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav_passo(&nav, leitura(false, true, true));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_INDEFINIDO, nav.lado);
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 0, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 0, NAV_PAREDE_O));
}

static void test_lado_fixado_nao_muda(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, true, false));
    posicionar(&nav, 0, 2, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, true));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
}

static void test_celula_atual_fica_visitada(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    TEST_ASSERT_FALSE(tem_parede(&nav, 0, 0, NAV_VISITADA));
    nav_passo(&nav, leitura(false, true, true));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 0, NAV_VISITADA));
    TEST_ASSERT_FALSE(tem_parede(&nav, 0, 1, NAV_VISITADA));
}

static void test_x_maior_ou_igual_a_4_resolve_longo_x_no_8x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 4, 1, NAV_LESTE);
    nav.mapa_alterado = false;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_LONGO_X, nav_orientacao(&nav));
    TEST_ASSERT_TRUE(nav.mapa_alterado);
}

static void test_y_maior_ou_igual_a_4_resolve_longo_y_no_8x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 4, NAV_NORTE);
    nav.mapa_alterado = false;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_LONGO_Y, nav_orientacao(&nav));
    TEST_ASSERT_TRUE(nav.mapa_alterado);
}

static void test_orientacao_resolvida_nao_muda(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 5, 0, NAV_LESTE);
    nav_passo(&nav, leitura(false, false, false));
    posicionar(&nav, 0, 6, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_LONGO_X, nav_orientacao(&nav));
}

static void test_4x4_nao_resolve_orientacao(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 3, 3, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_INDEFINIDA, nav_orientacao(&nav));
}

static void test_vizinha_fora_da_regiao_conta_como_parede(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    TEST_ASSERT_TRUE(nav_acessivel(&nav, 2, 1, NAV_LESTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 3, 1, NAV_LESTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 1, 3, NAV_NORTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 0, 1, NAV_OESTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 1, 0, NAV_SUL));
}

static void test_regiao_em_l_no_8x4_indefinido(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    TEST_ASSERT_TRUE(nav_acessivel(&nav, 6, 3, NAV_LESTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 7, 3, NAV_LESTE));
    TEST_ASSERT_TRUE(nav_acessivel(&nav, 3, 6, NAV_NORTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 3, 7, NAV_NORTE));
    /* O canto entre os dois braços do "L" não existe. */
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 4, 3, NAV_NORTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 3, 4, NAV_LESTE));
}

static void test_regiao_vira_retangulo_depois_de_resolvida(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 4, 0, NAV_LESTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_TRUE(nav_acessivel(&nav, 10, 3, NAV_LESTE));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 1, 3, NAV_NORTE));
}

static void test_leitura_aberta_para_fora_da_regiao_continua_parede(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 3, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_FALSE(nav_acessivel(&nav, 3, 1, NAV_LESTE));
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
    RUN_TEST(test_largada_grava_as_bordas_sul_e_oeste);
    RUN_TEST(test_parede_gravada_nos_dois_lados);
    RUN_TEST(test_leitura_com_rumo_leste_gira_as_paredes);
    RUN_TEST(test_lado_esquerda_espelha_as_leituras_laterais);
    RUN_TEST(test_leitura_aberta_nao_apaga_parede);
    RUN_TEST(test_parede_ja_conhecida_nao_liga_a_flag);
    RUN_TEST(test_so_a_direita_aberta_fixa_o_lado_direita);
    RUN_TEST(test_so_a_esquerda_aberta_fixa_o_lado_esquerda);
    RUN_TEST(test_duas_laterais_abertas_fixam_o_lado_direita);
    RUN_TEST(test_duas_laterais_fechadas_gravam_o_leste);
    RUN_TEST(test_lado_fixado_nao_muda);
    RUN_TEST(test_celula_atual_fica_visitada);
    RUN_TEST(test_x_maior_ou_igual_a_4_resolve_longo_x_no_8x4);
    RUN_TEST(test_y_maior_ou_igual_a_4_resolve_longo_y_no_8x4);
    RUN_TEST(test_orientacao_resolvida_nao_muda);
    RUN_TEST(test_4x4_nao_resolve_orientacao);
    RUN_TEST(test_vizinha_fora_da_regiao_conta_como_parede);
    RUN_TEST(test_regiao_em_l_no_8x4_indefinido);
    RUN_TEST(test_regiao_vira_retangulo_depois_de_resolvida);
    RUN_TEST(test_leitura_aberta_para_fora_da_regiao_continua_parede);
    RUN_TEST(test_direcao_depois_de_cada_acao);
    return UNITY_END();
}
