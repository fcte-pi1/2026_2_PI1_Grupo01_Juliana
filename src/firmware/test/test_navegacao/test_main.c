/*
 * Estes testes compilam no PC, onde não existe Arduino.h: se alguém incluir
 * Arduino na pasta navegacao/, este ambiente quebra.
 */

#include <stdio.h>
#include <string.h>
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

/*
 * A flag de mapa alterado é desligada pelo recálculo dentro de nav_passo: o
 * teste adultera a distância de (2, 2) antes e vê se o flood fill a refez.
 */
static bool mapa_foi_recalculado(const nav_estado_t *nav)
{
    return nav->mapa[2][2].distancia != 99 && !nav->mapa_alterado;
}

/*
 * Põe o robô numa célula e rumo quaisquer, como se já tivesse andado até lá.
 * Desliga o movimento pendente: a travessia não vale para uma pose escrita à mão.
 */
static void posicionar(nav_estado_t *nav, int x, int y, nav_direcao_t direcao)
{
    nav->x = (int8_t)x;
    nav->y = (int8_t)y;
    nav->direcao = direcao;
    nav->movimento_pendente = false;
}

/*
 * Grava à mão uma parede com votos, nos dois lados: com vantagem de 2 votos
 * ou mais ela não é perdoada (D14). A célula e a vizinha ficam na matriz.
 * Liga a flag de mapa alterado, como a leitura que a gravasse.
 */
static void fechar_com_votos(nav_estado_t *nav, int x, int y, nav_direcao_t direcao,
                             uint8_t votos)
{
    static const uint8_t bit[4] = {NAV_PAREDE_N, NAV_PAREDE_L, NAV_PAREDE_S, NAV_PAREDE_O};
    static const int8_t passo_x[4] = {0, 1, 0, -1};
    static const int8_t passo_y[4] = {1, 0, -1, 0};
    nav->mapa[x][y].paredes |= bit[direcao];
    nav->mapa[x + passo_x[direcao]][y + passo_y[direcao]].paredes |= bit[(direcao + 2) % 4];
    nav_parede_t *parede;
    switch (direcao) {
    case NAV_NORTE:
        parede = &nav->paredes[x][y][0];
        break;
    case NAV_LESTE:
        parede = &nav->paredes[x][y][1];
        break;
    case NAV_SUL:
        parede = &nav->paredes[x][y - 1][0];
        break;
    default:
        parede = &nav->paredes[x - 1][y][1];
        break;
    }
    parede->votos_parede = votos;
    nav->mapa_alterado = true;
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
    nav.mapa[2][2].distancia = 99;
    nav_passo(&nav, leitura(true, true, true));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 2, NAV_PAREDE_S));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
    TEST_ASSERT_TRUE(mapa_foi_recalculado(&nav));
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

/* Lê só a frente em (1, 1), rumo norte, quantas vezes a sequência pedir. */
static void ler_frente_em_1_1(nav_estado_t *nav, const char *sequencia)
{
    nav->lado = NAV_LADO_DIREITA;
    for (const char *c = sequencia; *c; c++) {
        posicionar(nav, 1, 1, NAV_NORTE);
        nav_passo(nav, leitura(*c == 'P', true, true));
    }
}

static void test_maioria_dos_votos_vence(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    ler_frente_em_1_1(&nav, "PPA");
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
}

static void test_empate_nos_votos_da_aberto(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    ler_frente_em_1_1(&nav, "PA");
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
}

static void test_leitura_aberta_apaga_parede_e_liga_a_flag(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    ler_frente_em_1_1(&nav, "P");
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.mapa[2][2].distancia = 99;
    nav_passo(&nav, leitura(false, true, true));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(mapa_foi_recalculado(&nav));
}

static void test_travessia_prova_aberto_mesmo_com_votos_de_parede_depois(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(&nav, leitura(false, true, true)));
    TEST_ASSERT_EQUAL_INT8(2, nav.y);
    /* A chamada seguinte, já em (1, 2), confirma a passagem por cima da parede sul. */
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_TRUE(nav.paredes[1][1][0].trocas & NAV_PROVADA_ABERTA);
    ler_frente_em_1_1(&nav, "PPP");
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
}

static void test_leitura_aberta_na_borda_nao_abre(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    for (int i = 0; i < 3; i++) {
        posicionar(&nav, 0, 1, NAV_NORTE);
        nav_passo(&nav, leitura(false, false, true));
    }
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_O));
}

static void test_na_quinta_mudanca_o_mapa_nao_muda_mais(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    ler_frente_em_1_1(&nav, "PAP");
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    ler_frente_em_1_1(&nav, "A");
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    /* Já foram 4 trocas: 3 votos de parede contra 2 não fecham mais. */
    ler_frente_em_1_1(&nav, "P");
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_EQUAL_UINT8(3, nav.paredes[1][1][0].votos_parede);
    TEST_ASSERT_EQUAL_UINT8(2, nav.paredes[1][1][0].votos_aberto);
}

static void test_votos_saturam_em_255(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    for (int i = 0; i < 300; i++) {
        ler_frente_em_1_1(&nav, "P");
    }
    TEST_ASSERT_EQUAL_UINT8(255, nav.paredes[1][1][0].votos_parede);
    TEST_ASSERT_EQUAL_UINT8(0, nav.paredes[1][1][0].votos_aberto);
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
}

static void test_parede_ja_conhecida_nao_liga_a_flag(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.lado = NAV_LADO_DIREITA;
    nav_passo(&nav, leitura(true, false, false));
    posicionar(&nav, 1, 1, NAV_NORTE);
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
    nav.mapa[2][2].distancia = 99;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_LONGO_X, nav_orientacao(&nav));
    TEST_ASSERT_TRUE(mapa_foi_recalculado(&nav));
}

static void test_y_maior_ou_igual_a_4_resolve_longo_y_no_8x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 4, NAV_NORTE);
    nav.mapa[2][2].distancia = 99;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_LONGO_Y, nav_orientacao(&nav));
    TEST_ASSERT_TRUE(mapa_foi_recalculado(&nav));
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

/* Confere as distâncias de um quadrado 4x4, linha a linha de y = 3 para y = 0. */
static void conferir_distancias_4x4(const nav_estado_t *nav, const uint8_t esperado[4][4])
{
    for (int linha = 0; linha < 4; linha++) {
        int y = 3 - linha;
        for (int x = 0; x < 4; x++) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(esperado[linha][x], nav_distancia(nav, x, y),
                                            "distancia diferente da tabela");
        }
    }
}

static void test_largada_ja_tem_as_distancias_calculadas(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 3, 3));
    TEST_ASSERT_EQUAL_UINT8(6, nav_distancia(&nav, 0, 0));
    TEST_ASSERT_FALSE(nav.mapa_alterado);
}

static void test_flood_fill_4x4_com_a_parede_leste_da_largada(void)
{
    static const uint8_t esperado[4][4] = {
        {3, 2, 1, 0},
        {4, 3, 2, 1},
        {5, 4, 3, 2},
        {6, 5, 4, 3},
    };
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav_passo(&nav, leitura(false, true, true));
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 0, NAV_PAREDE_L));
    conferir_distancias_4x4(&nav, esperado);
}

static void test_flood_fill_contorna_parede_conhecida(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 2, 3, NAV_LESTE);
    nav_passo(&nav, leitura(true, false, false));
    /* Com a parede entre (2, 3) e (3, 3), o caminho desce por (2, 2). */
    TEST_ASSERT_EQUAL_UINT8(3, nav_distancia(&nav, 2, 3));
    TEST_ASSERT_EQUAL_UINT8(1, nav_distancia(&nav, 3, 2));
}

/*
 * Exemplo resolvido do docs/4.4 (labirinto 4x4), com o lado longo à direita:
 * bordas, parede leste da largada e parede entre (2, 3) e (3, 3).
 */
static bool parede_do_exemplo(int x, int y, nav_direcao_t direcao)
{
    switch (direcao) {
    case NAV_NORTE: return y == 3;
    case NAV_SUL:   return y == 0;
    case NAV_OESTE: return x == 0 || (x == 3 && y == 3);
    case NAV_LESTE: return x == 3 || (x == 0 && y == 0) || (x == 2 && y == 3);
    }
    return true;
}

/* O que os sensores veem na célula atual do exemplo, relativo ao rumo. */
static nav_leitura_t leitura_do_exemplo(const nav_estado_t *nav)
{
    nav_direcao_t frente = nav->direcao;
    nav_direcao_t direita = nav_direcao_apos(frente, NAV_ACAO_DIREITA);
    nav_direcao_t esquerda = nav_direcao_apos(frente, NAV_ACAO_ESQUERDA);
    return leitura(parede_do_exemplo(nav->x, nav->y, frente),
                   parede_do_exemplo(nav->x, nav->y, esquerda),
                   parede_do_exemplo(nav->x, nav->y, direita));
}

static void test_exemplo_4x4_do_documento(void)
{
    static const uint8_t antes_da_parede[4][4] = {
        {3, 2, 1, 0},
        {4, 3, 2, 1},
        {5, 4, 3, 2},
        {6, 5, 4, 3},
    };
    static const uint8_t depois_da_parede[4][4] = {
        {5, 4, 3, 0},
        {4, 3, 2, 1},
        {5, 4, 3, 2},
        {6, 5, 4, 3},
    };
    static const int8_t trajeto[][2] = {
        {0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {2, 2}, {3, 2}, {3, 3},
    };
    const int celulas = (int)(sizeof(trajeto) / sizeof(trajeto[0]));
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav_acao_t acao = NAV_ACAO_FRENTE;
    for (int i = 0; i < celulas; i++) {
        TEST_ASSERT_EQUAL_INT8_MESSAGE(trajeto[i][0], nav.x, "trajeto diferente do 4.4");
        TEST_ASSERT_EQUAL_INT8_MESSAGE(trajeto[i][1], nav.y, "trajeto diferente do 4.4");
        int x = nav.x;
        int y = nav.y;
        acao = nav_passo(&nav, leitura_do_exemplo(&nav));
        if (x == 0 && y == 1) {
            conferir_distancias_4x4(&nav, antes_da_parede);
        }
        if (x == 2 && y == 3) {
            conferir_distancias_4x4(&nav, depois_da_parede);
        }
        if (i < celulas - 1) {
            TEST_ASSERT_TRUE_MESSAGE(acao < NAV_ACAO_SUCESSO, "terminou antes de (3, 3)");
        }
    }
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SUCESSO, acao);
}

static void test_8x4_indefinido_tem_os_dois_objetivos(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 7, 3));
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 3, 7));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 5, 5));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 7, 7));
    TEST_ASSERT_EQUAL_UINT8(10, nav_distancia(&nav, 0, 0));
}

static void test_12x4_indefinido_tem_os_dois_objetivos(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 11, 3));
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 3, 11));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 11, 11));
    TEST_ASSERT_EQUAL_UINT8(14, nav_distancia(&nav, 0, 0));
}

static void test_orientacao_resolvida_descarta_o_objetivo_incompativel(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 4, 0, NAV_LESTE);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_EQUAL_UINT8(0, nav_distancia(&nav, 7, 3));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 3, 7));
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 1, 5));
    TEST_ASSERT_FALSE(nav.mapa_alterado);
}

static void test_celula_isolada_fica_infinita(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    fechar_com_votos(&nav, 1, 1, NAV_NORTE, 2);
    fechar_com_votos(&nav, 1, 1, NAV_LESTE, 2);
    fechar_com_votos(&nav, 1, 1, NAV_SUL, 2);
    fechar_com_votos(&nav, 1, 1, NAV_OESTE, 2);
    nav_recalcular(&nav);
    TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav_distancia(&nav, 1, 1));
}

static void test_mapa_sem_mudanca_nao_recalcula(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav.mapa[2][2].distancia = 99;
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_FALSE(mapa_foi_recalculado(&nav));
    nav_passo(&nav, leitura(true, false, false));
    TEST_ASSERT_TRUE(mapa_foi_recalculado(&nav));
    TEST_ASSERT_EQUAL_UINT8(2, nav_distancia(&nav, 2, 2));
}

static void test_empate_entre_frente_e_giro_fica_com_a_frente(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    /* (1, 2) e (2, 1) valem 3: a frente vence o giro. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(&nav, leitura(false, false, false)));
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(2, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav.direcao);
}

/*
 * Frente fechada e as duas laterais com o mesmo valor. A parede já está no
 * mapa, então a leitura não muda nada e as distâncias postas à mão ficam.
 */
static void preparar_empate_entre_os_giros(nav_estado_t *nav, nav_lado_t lado)
{
    nav_iniciar(nav, NAV_TIPO_4X4);
    nav->lado = lado;
    posicionar(nav, 1, 1, NAV_NORTE);
    nav->mapa[1][1].paredes |= NAV_PAREDE_N;
    nav->mapa[1][2].paredes |= NAV_PAREDE_S;
    nav->mapa[1][1].distancia = 4;
    nav->mapa[0][1].distancia = 3;
    nav->mapa[2][1].distancia = 3;
}

static void test_empate_entre_os_giros_fica_com_o_horario_do_labirinto(void)
{
    nav_estado_t nav;
    preparar_empate_entre_os_giros(&nav, NAV_LADO_DIREITA);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, nav_passo(&nav, leitura(true, false, false)));
    TEST_ASSERT_EQUAL_INT8(2, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav.direcao);
}

static void test_lado_esquerda_devolve_o_giro_horario_como_esquerda(void)
{
    nav_estado_t nav;
    preparar_empate_entre_os_giros(&nav, NAV_LADO_ESQUERDA);
    /* O leste do labirinto fica à esquerda do robô. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_ESQUERDA, nav_passo(&nav, leitura(true, false, false)));
    TEST_ASSERT_EQUAL_INT8(2, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav.direcao);
}

static void test_beco_da_meia_volta(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_MEIA_VOLTA, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav.direcao);
}

static void test_sucesso_na_celula_de_objetivo(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 3, 3, NAV_NORTE);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SUCESSO, nav_passo(&nav, leitura(true, false, true)));
    TEST_ASSERT_TRUE(nav.terminou);
    TEST_ASSERT_EQUAL_INT8(3, nav.x);
    TEST_ASSERT_EQUAL_INT8(3, nav.y);
}

static void test_sem_passo_quando_fica_cercado(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    fechar_com_votos(&nav, 1, 1, NAV_NORTE, 1);
    fechar_com_votos(&nav, 1, 1, NAV_LESTE, 1);
    fechar_com_votos(&nav, 1, 1, NAV_SUL, 2);
    fechar_com_votos(&nav, 1, 1, NAV_OESTE, 1);
    /* As três lidas agora chegam a 2 votos: nenhuma parede é perdoável. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_TRUE(nav.terminou);
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
}

/*
 * (1, 1) com as paredes leste, sul e oeste firmes: a única saída é o norte.
 * Uma leitura de parede na frente fecha o caminho com vantagem de 1 voto.
 */
static void preparar_beco_com_a_frente_duvidosa(nav_estado_t *nav)
{
    nav_iniciar(nav, NAV_TIPO_4X4);
    nav->lado = NAV_LADO_DIREITA;
    fechar_com_votos(nav, 1, 1, NAV_LESTE, 2);
    fechar_com_votos(nav, 1, 1, NAV_SUL, 2);
    fechar_com_votos(nav, 1, 1, NAV_OESTE, 2);
    posicionar(nav, 1, 1, NAV_NORTE);
}

static void test_parede_lida_uma_vez_que_fecha_o_caminho_e_perdoada(void)
{
    nav_estado_t nav;
    preparar_beco_com_a_frente_duvidosa(&nav);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(2, nav.y);
    TEST_ASSERT_EQUAL_UINT8(1, nav.perdoes);
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
    TEST_ASSERT_EQUAL_UINT8(nav.paredes[1][1][0].votos_aberto, nav.paredes[1][1][0].votos_parede);
    /* O perdão não conta troca: só a leitura que fechou a parede contou. */
    TEST_ASSERT_EQUAL_UINT8(1, nav.paredes[1][1][0].trocas & NAV_TROCAS_MASCARA);
    /* As firmes ficam. */
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_S));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
}

static void test_parede_com_vantagem_de_2_votos_nao_e_perdoada(void)
{
    nav_estado_t nav;
    preparar_beco_com_a_frente_duvidosa(&nav);
    fechar_com_votos(&nav, 1, 1, NAV_NORTE, 1);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_UINT8(0, nav.perdoes);
    TEST_ASSERT_EQUAL_UINT8(2, nav.paredes[1][1][0].votos_parede);
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
}

static void test_depois_de_8_perdoes_sem_passo(void)
{
    nav_estado_t nav;
    preparar_beco_com_a_frente_duvidosa(&nav);
    nav.perdoes = NAV_PERDOES_MAX - 1;
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_UINT8(NAV_PERDOES_MAX, nav.perdoes);
    posicionar(&nav, 1, 1, NAV_NORTE);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_UINT8(NAV_PERDOES_MAX, nav.perdoes);
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
}

static void test_cercado_por_bordas_e_paredes_firmes_termina_sem_passo(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    /* Na largada, o sul e o oeste são borda: nunca perdoados. */
    fechar_com_votos(&nav, 0, 0, NAV_NORTE, 2);
    fechar_com_votos(&nav, 0, 0, NAV_LESTE, 2);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_passo(&nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_UINT8(0, nav.perdoes);
    TEST_ASSERT_TRUE(nav.terminou);
}

static void conferir_fim_idempotente(nav_estado_t *nav, nav_acao_t fim)
{
    nav_estado_t antes;
    memcpy(&antes, nav, sizeof(antes));
    TEST_ASSERT_EQUAL_INT(fim, nav_passo(nav, leitura(false, false, false)));
    TEST_ASSERT_EQUAL_INT(fim, nav_passo(nav, leitura(true, true, true)));
    TEST_ASSERT_EQUAL_MEMORY(&antes, nav, sizeof(antes));
}

static void test_sucesso_e_idempotente(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 3, 3, NAV_NORTE);
    nav_passo(&nav, leitura(true, false, true));
    conferir_fim_idempotente(&nav, NAV_ACAO_SUCESSO);
}

static void test_sem_passo_e_idempotente(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    fechar_com_votos(&nav, 1, 1, NAV_NORTE, 1);
    fechar_com_votos(&nav, 1, 1, NAV_LESTE, 1);
    fechar_com_votos(&nav, 1, 1, NAV_SUL, 2);
    fechar_com_votos(&nav, 1, 1, NAV_OESTE, 1);
    nav_passo(&nav, leitura(true, true, true));
    conferir_fim_idempotente(&nav, NAV_ACAO_SEM_PASSO);
}

/*
 * Em (1, 1), rumo norte, tudo aberto: a navegação manda seguir em frente e
 * já deixa a pose em (1, 2). O app, parado, ainda pode conferir a frente.
 */
static void mandar_em_frente_de_1_1(nav_estado_t *nav)
{
    nav_iniciar(nav, NAV_TIPO_4X4);
    nav->lado = NAV_LADO_DIREITA;
    posicionar(nav, 1, 1, NAV_NORTE);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(nav, leitura(false, false, false)));
}

static void test_bloqueio_grava_a_parede_volta_a_pose_e_gira(void)
{
    nav_estado_t nav;
    mandar_em_frente_de_1_1(&nav);
    /* O voto de parede empata com o de aberto da leitura: a frente continua. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(2, nav.y);
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    /* A segunda conferência fecha a parede e o robô vira para (2, 1). */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, nav_bloqueado(&nav));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
    TEST_ASSERT_EQUAL_UINT8(2, nav.paredes[1][1][0].votos_parede);
    TEST_ASSERT_EQUAL_INT8(2, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav.direcao);
    TEST_ASSERT_TRUE(nav.movimento_pendente);
    TEST_ASSERT_EQUAL_UINT8(2, nav.bloqueios);
    TEST_ASSERT_FALSE(nav.terminou);
}

static void test_bloqueio_seguido_de_passo_prova_aberta_a_parede_do_giro(void)
{
    nav_estado_t nav;
    mandar_em_frente_de_1_1(&nav);
    nav_bloqueado(&nav);
    nav_bloqueado(&nav);
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_TRUE(nav.paredes[1][1][1].trocas & NAV_PROVADA_ABERTA);
    TEST_ASSERT_FALSE(nav.paredes[1][1][0].trocas & NAV_PROVADA_ABERTA);
}

/*
 * Defeito visto no 12x4: a parede da frente já tinha trocado 4 vezes e
 * estava aberta no mapa, então nenhuma leitura a fechava e o robô batia.
 */
static void test_bloqueio_fecha_parede_que_ja_trocou_4_vezes(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    nav.paredes[1][1][0].votos_parede = 2;
    nav.paredes[1][1][0].votos_aberto = 3;
    nav.paredes[1][1][0].trocas = NAV_TROCAS_MAX;
    posicionar(&nav, 1, 1, NAV_NORTE);
    /* A leitura de parede empata os votos, e o limite de trocas a ignoraria. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_passo(&nav, leitura(true, false, false)));
    TEST_ASSERT_FALSE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, nav_bloqueado(&nav));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_N));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 2, NAV_PAREDE_S));
    TEST_ASSERT_EQUAL_INT8(2, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
}

static void test_depois_de_64_bloqueios_sem_passo(void)
{
    nav_estado_t nav;
    mandar_em_frente_de_1_1(&nav);
    nav.bloqueios = NAV_BLOQUEIOS_MAX - 1;
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_FRENTE, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_UINT8(NAV_BLOQUEIOS_MAX, nav.bloqueios);
    TEST_ASSERT_FALSE(nav.terminou);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_bloqueado(&nav));
    TEST_ASSERT_TRUE(nav.terminou);
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav.acao_terminal);
}

static void test_bloqueio_depois_do_fim_e_idempotente(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 3, 3, NAV_NORTE);
    nav_passo(&nav, leitura(true, false, true));
    nav.movimento_pendente = true;
    nav_estado_t antes;
    memcpy(&antes, &nav, sizeof(antes));
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SUCESSO, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SUCESSO, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_MEMORY(&antes, &nav, sizeof(antes));
}

static void test_bloqueio_sem_movimento_pendente_nao_muda_o_estado(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    nav.lado = NAV_LADO_DIREITA;
    posicionar(&nav, 1, 1, NAV_NORTE);
    nav_estado_t antes;
    memcpy(&antes, &nav, sizeof(antes));
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SEM_PASSO, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_MEMORY(&antes, &nav, sizeof(antes));
}

/*
 * Em (0, 1), rumo norte, frente fechada: a leitura fixa o lado e a navegação
 * manda virar para o leste. O app, já virado, ainda não andou.
 */
static nav_acao_t fixar_lado_e_virar_para_o_leste(nav_estado_t *nav, nav_leitura_t lida)
{
    nav_iniciar(nav, NAV_TIPO_4X4);
    posicionar(nav, 0, 1, NAV_NORTE);
    nav_acao_t acao = nav_passo(nav, lida);
    TEST_ASSERT_EQUAL_INT8(1, nav->x);
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav->direcao);
    return acao;
}

static void test_fixar_o_lado_guarda_a_celula_e_as_laterais(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(true, false, false));
    TEST_ASSERT_TRUE(nav.lado_provisorio);
    TEST_ASSERT_EQUAL_INT8(1, nav.lado_y);
    TEST_ASSERT_TRUE(nav.lado_duas_abertas);
    TEST_ASSERT_EQUAL_UINT8(0, nav.desfazeres);
}

static void test_bloqueio_com_a_outra_lateral_aberta_troca_o_lado(void)
{
    nav_estado_t nav;
    /* As duas laterais abertas: o lado vira direita e o leste é a direita física. */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, fixar_lado_e_virar_para_o_leste(&nav, leitura(true, false, false)));
    /*
     * Com o lado trocado, o robô virado para a direita física olha para o
     * oeste: a meia-volta o leva para a esquerda física, o leste de verdade.
     */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_MEIA_VOLTA, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_ESQUERDA, nav.lado);
    TEST_ASSERT_TRUE(nav.lado_provisorio);
    TEST_ASSERT_FALSE(nav.lado_duas_abertas);
    TEST_ASSERT_EQUAL_UINT8(1, nav.desfazeres);
    TEST_ASSERT_EQUAL_INT8(1, nav.x);
    TEST_ASSERT_EQUAL_INT8(1, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav.direcao);
    TEST_ASSERT_TRUE(nav.movimento_pendente);
}

static void test_bloqueio_com_so_a_direita_aberta_volta_o_lado_a_indefinido(void)
{
    nav_estado_t nav;
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, fixar_lado_e_virar_para_o_leste(&nav, leitura(true, true, false)));
    /*
     * Indefinido espelha como direita: o robô virado para a direita física
     * olha para o leste, que ganha 2 votos de parede. Sobra o sul, à direita.
     */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_DIREITA, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_INDEFINIDO, nav.lado);
    TEST_ASSERT_EQUAL_UINT8(2, nav.paredes[0][1][1].votos_parede);
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
    TEST_ASSERT_TRUE(tem_parede(&nav, 1, 1, NAV_PAREDE_O));
    TEST_ASSERT_EQUAL_UINT8(1, nav.desfazeres);
    TEST_ASSERT_EQUAL_INT8(0, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav.direcao);
}

static void test_bloqueio_com_so_a_esquerda_aberta_volta_o_lado_a_indefinido(void)
{
    nav_estado_t nav;
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_ESQUERDA, fixar_lado_e_virar_para_o_leste(&nav, leitura(true, false, true)));
    /*
     * O robô virado para a esquerda física olha para o oeste quando o lado
     * espelha como direita: a borda. O leste, atrás dele, é a direita física,
     * que foi lida fechada e ganha 1 voto de parede. Sobra o sul, à esquerda.
     */
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_ESQUERDA, nav_bloqueado(&nav));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_INDEFINIDO, nav.lado);
    TEST_ASSERT_EQUAL_UINT8(1, nav.paredes[0][1][1].votos_parede);
    TEST_ASSERT_TRUE(tem_parede(&nav, 0, 1, NAV_PAREDE_L));
    TEST_ASSERT_EQUAL_INT8(0, nav.x);
    TEST_ASSERT_EQUAL_INT8(0, nav.y);
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav.direcao);
}

static void test_desfazer_o_lado_apaga_as_paredes_leste_da_coluna_0(void)
{
    nav_estado_t nav;
    fixar_lado_e_virar_para_o_leste(&nav, leitura(true, false, false));
    fechar_com_votos(&nav, 0, 2, NAV_LESTE, 3);
    nav.paredes[0][2][1].votos_aberto = 1;
    nav.paredes[0][2][1].trocas = 2;
    nav_bloqueado(&nav);
    for (int y = 0; y < NAV_MAX_CELULAS; y++) {
        TEST_ASSERT_EQUAL_UINT8(0, nav.paredes[0][y][1].votos_parede);
        TEST_ASSERT_EQUAL_UINT8(0, nav.paredes[0][y][1].votos_aberto);
        TEST_ASSERT_EQUAL_UINT8(0, nav.paredes[0][y][1].trocas);
        TEST_ASSERT_FALSE(tem_parede(&nav, 0, y, NAV_PAREDE_L));
        TEST_ASSERT_FALSE(tem_parede(&nav, 1, y, NAV_PAREDE_O));
    }
}

static void test_depois_de_4_desfazeres_o_bloqueio_so_vota(void)
{
    nav_estado_t nav;
    fixar_lado_e_virar_para_o_leste(&nav, leitura(true, false, false));
    nav.desfazeres = NAV_DESFAZERES_MAX;
    nav_bloqueado(&nav);
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
    TEST_ASSERT_EQUAL_UINT8(1, nav.paredes[0][1][1].votos_parede);
}

static void test_sair_da_coluna_0_torna_o_lado_definitivo(void)
{
    nav_estado_t nav;
    fixar_lado_e_virar_para_o_leste(&nav, leitura(true, false, false));
    nav_passo(&nav, leitura(false, false, false));
    TEST_ASSERT_FALSE(nav.lado_provisorio);
    /* De volta à célula onde o lado foi fixado, um bloqueio para o leste só vota. */
    posicionar(&nav, 1, 1, NAV_LESTE);
    nav.movimento_pendente = true;
    nav_bloqueado(&nav);
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
    TEST_ASSERT_EQUAL_UINT8(0, nav.desfazeres);
}

/*
 * Defeito visto no 12x4: o robô saiu da coluna 0 com o lado indefinido e, ao
 * voltar, uma abertura lateral falsa fixava o lado com o espelho trocado.
 */
static void test_sair_da_coluna_0_com_lado_indefinido_fixa_direita(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    posicionar(&nav, 1, 0, NAV_LESTE);
    nav_passo(&nav, leitura(false, true, true));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
    TEST_ASSERT_FALSE(nav.lado_provisorio);
    posicionar(&nav, 0, 1, NAV_NORTE);
    nav_passo(&nav, leitura(false, false, true));
    TEST_ASSERT_EQUAL_INT(NAV_LADO_DIREITA, nav.lado);
}

/*
 * Labirinto 4x4 do lado esquerdo do robô, no referencial do labirinto: bordas,
 * parede leste de (0, 0) e parede norte de (0, 1).
 */
static bool parede_a_esquerda(int x, int y, nav_direcao_t direcao)
{
    switch (direcao) {
    case NAV_NORTE: return y == 3 || (x == 0 && y == 1);
    case NAV_SUL:   return y == 0 || (x == 0 && y == 2);
    case NAV_OESTE: return x == 0 || (x == 1 && y == 0);
    case NAV_LESTE: return x == 3 || (x == 0 && y == 0);
    }
    return true;
}

/*
 * Defeito visto no 4x4: em (0, 1) a direita, que é borda, foi lida aberta.
 * O lado fixava como direita, o robô virava para a borda e nunca chegava.
 * O teste segue o robô de verdade (com o lado esquerda) e confere a frente
 * antes de cada passo, como o app.
 */
static void test_leitura_falsa_a_direita_com_o_labirinto_a_esquerda_chega(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    int x = 0;
    int y = 0;
    nav_direcao_t rumo = NAV_NORTE;
    nav_acao_t acao = NAV_ACAO_FRENTE;
    int bloqueios = 0;
    for (int passo = 0; passo < 50; passo++) {
        /* Com o labirinto à esquerda, a direita física é o giro anti-horário. */
        nav_direcao_t direita = nav_direcao_apos(rumo, NAV_ACAO_ESQUERDA);
        nav_direcao_t esquerda = nav_direcao_apos(rumo, NAV_ACAO_DIREITA);
        nav_leitura_t lida = leitura(parede_a_esquerda(x, y, rumo),
                                     parede_a_esquerda(x, y, esquerda),
                                     parede_a_esquerda(x, y, direita));
        if (x == 0 && y == 1) {
            lida.direita = false;
        }
        acao = nav_passo(&nav, lida);
        if (acao >= NAV_ACAO_SUCESSO) {
            break;
        }
        for (;;) {
            if (acao == NAV_ACAO_DIREITA) {
                rumo = nav_direcao_apos(rumo, NAV_ACAO_ESQUERDA);
            } else if (acao == NAV_ACAO_ESQUERDA) {
                rumo = nav_direcao_apos(rumo, NAV_ACAO_DIREITA);
            } else {
                rumo = nav_direcao_apos(rumo, acao);
            }
            if (!parede_a_esquerda(x, y, rumo)) {
                break;
            }
            bloqueios++;
            acao = nav_bloqueado(&nav);
            TEST_ASSERT_TRUE_MESSAGE(acao < NAV_ACAO_SUCESSO, "terminou num bloqueio");
        }
        x += rumo == NAV_LESTE ? 1 : rumo == NAV_OESTE ? -1 : 0;
        y += rumo == NAV_NORTE ? 1 : rumo == NAV_SUL ? -1 : 0;
    }
    TEST_ASSERT_EQUAL_INT(NAV_ACAO_SUCESSO, acao);
    TEST_ASSERT_EQUAL_INT(3, x);
    TEST_ASSERT_EQUAL_INT(3, y);
    TEST_ASSERT_EQUAL_INT(1, bloqueios);
    TEST_ASSERT_EQUAL_INT(NAV_LADO_ESQUERDA, nav.lado);
}

static void test_direcao_depois_de_cada_acao(void)
{
    TEST_ASSERT_EQUAL_INT(NAV_LESTE, nav_direcao_apos(NAV_NORTE, NAV_ACAO_DIREITA));
    TEST_ASSERT_EQUAL_INT(NAV_OESTE, nav_direcao_apos(NAV_NORTE, NAV_ACAO_ESQUERDA));
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav_direcao_apos(NAV_NORTE, NAV_ACAO_MEIA_VOLTA));
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, nav_direcao_apos(NAV_OESTE, NAV_ACAO_DIREITA));
    TEST_ASSERT_EQUAL_INT(NAV_SUL, nav_direcao_apos(NAV_SUL, NAV_ACAO_FRENTE));
}

/*
 * Equivalência do flood fill (#224): a versão por varreduras de Harrison [9]
 * fica aqui como referência, e o mapa inteiro de distâncias de nav_recalcular
 * é comparado com ela célula a célula. Região e objetivos são cópias das
 * regras static do nav.c (D6).
 */
static bool regiao_da_referencia(const nav_estado_t *nav, int x, int y)
{
    if (x < 0 || y < 0) {
        return false;
    }
    int longo = nav->tipo == NAV_TIPO_12X4 ? 12 : nav->tipo == NAV_TIPO_8X4 ? 8 : 4;
    bool longo_x = x < longo && y < 4;
    bool longo_y = x < 4 && y < longo;
    switch (nav->orientacao) {
    case NAV_ORIENTACAO_LONGO_X:
        return longo_x;
    case NAV_ORIENTACAO_LONGO_Y:
        return longo_y;
    default:
        return longo_x || longo_y;
    }
}

static bool objetivo_da_referencia(const nav_estado_t *nav, int x, int y)
{
    if (!regiao_da_referencia(nav, x, y)) {
        return false;
    }
    int ultimo_longo = (nav->tipo == NAV_TIPO_12X4 ? 12 : nav->tipo == NAV_TIPO_8X4 ? 8 : 4) - 1;
    return (x == ultimo_longo && y == 3) || (x == 3 && y == ultimo_longo);
}

/* Varreduras da matriz inteira até nada mudar, sem tocar no estado. */
static void recalcular_por_varreduras(const nav_estado_t *nav,
                                      uint8_t referencia[NAV_MAX_CELULAS][NAV_MAX_CELULAS])
{
    static const int passo_x[4] = {0, 1, 0, -1};
    static const int passo_y[4] = {1, 0, -1, 0};
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            referencia[x][y] = objetivo_da_referencia(nav, x, y) ? 0 : NAV_DIST_INFINITA;
        }
    }

    bool mudou = true;
    while (mudou) {
        mudou = false;
        for (int x = 0; x < NAV_MAX_CELULAS; x++) {
            for (int y = 0; y < NAV_MAX_CELULAS; y++) {
                if (!regiao_da_referencia(nav, x, y) || objetivo_da_referencia(nav, x, y)) {
                    continue;
                }
                uint8_t menor = NAV_DIST_INFINITA;
                for (int d = NAV_NORTE; d <= NAV_OESTE; d++) {
                    if (!nav_acessivel(nav, x, y, (nav_direcao_t)d)) {
                        continue;
                    }
                    uint8_t vizinha = referencia[x + passo_x[d]][y + passo_y[d]];
                    if (vizinha < menor) {
                        menor = vizinha;
                    }
                }
                uint8_t valor = menor == NAV_DIST_INFINITA ? NAV_DIST_INFINITA : (uint8_t)(menor + 1);
                if (valor != referencia[x][y]) {
                    referencia[x][y] = valor;
                    mudou = true;
                }
            }
        }
    }
}

static void conferir_equivalencia(nav_estado_t *nav)
{
    uint8_t referencia[NAV_MAX_CELULAS][NAV_MAX_CELULAS];
    nav_recalcular(nav);
    recalcular_por_varreduras(nav, referencia);
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            char mensagem[48];
            snprintf(mensagem, sizeof mensagem, "distancia diferente em x = %d, y = %d", x, y);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(referencia[x][y], nav->mapa[x][y].distancia, mensagem);
        }
    }
}

/* Parede entre (x, y) e (x, y + 1), gravada nos dois lados. */
static void parede_ao_norte(nav_estado_t *nav, int x, int y)
{
    nav->mapa[x][y].paredes |= NAV_PAREDE_N;
    nav->mapa[x][y + 1].paredes |= NAV_PAREDE_S;
}

/* Parede entre (x, y) e (x + 1, y), gravada nos dois lados. */
static void parede_a_leste(nav_estado_t *nav, int x, int y)
{
    nav->mapa[x][y].paredes |= NAV_PAREDE_L;
    nav->mapa[x + 1][y].paredes |= NAV_PAREDE_O;
}

static void test_equivalencia_no_labirinto_vazio_4x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    conferir_equivalencia(&nav);
}

static void test_equivalencia_no_labirinto_vazio_8x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_8X4);
    conferir_equivalencia(&nav);
}

static void test_equivalencia_no_labirinto_vazio_12x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    conferir_equivalencia(&nav);
}

/* A serpentina do test_desempenho_nav: 12x4, orientação indefinida. */
static void test_equivalencia_na_serpentina_12x4(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_12X4);
    for (int y = 0; y < NAV_MAX_CELULAS - 1; y++) {
        int largura = y < 3 ? NAV_MAX_CELULAS : 4;
        int passagem = y % 2 == 0 ? largura - 1 : 0;
        for (int x = 0; x < largura; x++) {
            if (x != passagem) {
                parede_ao_norte(&nav, x, y);
            }
        }
    }
    conferir_equivalencia(&nav);
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_INDEFINIDA, nav_orientacao(&nav));
}

static void conferir_orientacao_forcada(nav_tipo_t tipo, nav_orientacao_t orientacao)
{
    nav_estado_t nav;
    nav_iniciar(&nav, tipo);
    nav.orientacao = orientacao;
    conferir_equivalencia(&nav);
}

static void test_equivalencia_8x4_com_longo_x(void)
{
    conferir_orientacao_forcada(NAV_TIPO_8X4, NAV_ORIENTACAO_LONGO_X);
}

static void test_equivalencia_8x4_com_longo_y(void)
{
    conferir_orientacao_forcada(NAV_TIPO_8X4, NAV_ORIENTACAO_LONGO_Y);
}

static void test_equivalencia_12x4_com_longo_x(void)
{
    conferir_orientacao_forcada(NAV_TIPO_12X4, NAV_ORIENTACAO_LONGO_X);
}

static void test_equivalencia_12x4_com_longo_y(void)
{
    conferir_orientacao_forcada(NAV_TIPO_12X4, NAV_ORIENTACAO_LONGO_Y);
}

/* LCG de 32 bits (constantes de Numerical Recipes), semente fixa: teste repetível. */
static uint32_t sorteio = 20261023u;

static uint32_t sortear(uint32_t limite)
{
    sorteio = sorteio * 1664525u + 1013904223u;
    return (sorteio >> 16) % limite;
}

#define LABIRINTOS_ALEATORIOS 200

/*
 * Para cada orientação, 200 labirintos com paredes ao norte e a leste
 * sorteadas na matriz inteira, com densidade entre 10% e 50%.
 */
static void conferir_aleatorios(nav_tipo_t tipo)
{
    static const nav_orientacao_t orientacoes[3] = {
        NAV_ORIENTACAO_INDEFINIDA, NAV_ORIENTACAO_LONGO_X, NAV_ORIENTACAO_LONGO_Y
    };
    for (int o = 0; o < 3; o++) {
        for (int i = 0; i < LABIRINTOS_ALEATORIOS; i++) {
            nav_estado_t nav;
            nav_iniciar(&nav, tipo);
            nav.orientacao = orientacoes[o];
            uint32_t densidade = 10 + sortear(41);
            for (int x = 0; x < NAV_MAX_CELULAS; x++) {
                for (int y = 0; y < NAV_MAX_CELULAS; y++) {
                    if (y + 1 < NAV_MAX_CELULAS && sortear(100) < densidade) {
                        parede_ao_norte(&nav, x, y);
                    }
                    if (x + 1 < NAV_MAX_CELULAS && sortear(100) < densidade) {
                        parede_a_leste(&nav, x, y);
                    }
                }
            }
            conferir_equivalencia(&nav);
        }
    }
}

static void test_equivalencia_em_labirintos_aleatorios_4x4(void)
{
    conferir_aleatorios(NAV_TIPO_4X4);
}

static void test_equivalencia_em_labirintos_aleatorios_8x4(void)
{
    conferir_aleatorios(NAV_TIPO_8X4);
}

static void test_equivalencia_em_labirintos_aleatorios_12x4(void)
{
    conferir_aleatorios(NAV_TIPO_12X4);
}

/* Objetivo (3, 3) do 4x4 fechado a sul e a oeste: ninguém chega a ele. */
static void test_equivalencia_com_o_objetivo_cercado(void)
{
    nav_estado_t nav;
    nav_iniciar(&nav, NAV_TIPO_4X4);
    parede_ao_norte(&nav, 3, 2);
    parede_a_leste(&nav, 2, 3);
    conferir_equivalencia(&nav);
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            if (x != 3 || y != 3) {
                TEST_ASSERT_EQUAL_UINT8(NAV_DIST_INFINITA, nav.mapa[x][y].distancia);
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT8(0, nav.mapa[3][3].distancia);
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
    RUN_TEST(test_maioria_dos_votos_vence);
    RUN_TEST(test_empate_nos_votos_da_aberto);
    RUN_TEST(test_leitura_aberta_apaga_parede_e_liga_a_flag);
    RUN_TEST(test_travessia_prova_aberto_mesmo_com_votos_de_parede_depois);
    RUN_TEST(test_leitura_aberta_na_borda_nao_abre);
    RUN_TEST(test_na_quinta_mudanca_o_mapa_nao_muda_mais);
    RUN_TEST(test_votos_saturam_em_255);
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
    RUN_TEST(test_largada_ja_tem_as_distancias_calculadas);
    RUN_TEST(test_flood_fill_4x4_com_a_parede_leste_da_largada);
    RUN_TEST(test_flood_fill_contorna_parede_conhecida);
    RUN_TEST(test_exemplo_4x4_do_documento);
    RUN_TEST(test_8x4_indefinido_tem_os_dois_objetivos);
    RUN_TEST(test_12x4_indefinido_tem_os_dois_objetivos);
    RUN_TEST(test_orientacao_resolvida_descarta_o_objetivo_incompativel);
    RUN_TEST(test_celula_isolada_fica_infinita);
    RUN_TEST(test_mapa_sem_mudanca_nao_recalcula);
    RUN_TEST(test_empate_entre_frente_e_giro_fica_com_a_frente);
    RUN_TEST(test_empate_entre_os_giros_fica_com_o_horario_do_labirinto);
    RUN_TEST(test_lado_esquerda_devolve_o_giro_horario_como_esquerda);
    RUN_TEST(test_beco_da_meia_volta);
    RUN_TEST(test_sucesso_na_celula_de_objetivo);
    RUN_TEST(test_sem_passo_quando_fica_cercado);
    RUN_TEST(test_parede_lida_uma_vez_que_fecha_o_caminho_e_perdoada);
    RUN_TEST(test_parede_com_vantagem_de_2_votos_nao_e_perdoada);
    RUN_TEST(test_depois_de_8_perdoes_sem_passo);
    RUN_TEST(test_cercado_por_bordas_e_paredes_firmes_termina_sem_passo);
    RUN_TEST(test_sucesso_e_idempotente);
    RUN_TEST(test_sem_passo_e_idempotente);
    RUN_TEST(test_bloqueio_grava_a_parede_volta_a_pose_e_gira);
    RUN_TEST(test_bloqueio_seguido_de_passo_prova_aberta_a_parede_do_giro);
    RUN_TEST(test_bloqueio_fecha_parede_que_ja_trocou_4_vezes);
    RUN_TEST(test_depois_de_64_bloqueios_sem_passo);
    RUN_TEST(test_bloqueio_depois_do_fim_e_idempotente);
    RUN_TEST(test_bloqueio_sem_movimento_pendente_nao_muda_o_estado);
    RUN_TEST(test_fixar_o_lado_guarda_a_celula_e_as_laterais);
    RUN_TEST(test_bloqueio_com_a_outra_lateral_aberta_troca_o_lado);
    RUN_TEST(test_bloqueio_com_so_a_direita_aberta_volta_o_lado_a_indefinido);
    RUN_TEST(test_bloqueio_com_so_a_esquerda_aberta_volta_o_lado_a_indefinido);
    RUN_TEST(test_desfazer_o_lado_apaga_as_paredes_leste_da_coluna_0);
    RUN_TEST(test_depois_de_4_desfazeres_o_bloqueio_so_vota);
    RUN_TEST(test_sair_da_coluna_0_torna_o_lado_definitivo);
    RUN_TEST(test_sair_da_coluna_0_com_lado_indefinido_fixa_direita);
    RUN_TEST(test_leitura_falsa_a_direita_com_o_labirinto_a_esquerda_chega);
    RUN_TEST(test_direcao_depois_de_cada_acao);
    RUN_TEST(test_equivalencia_no_labirinto_vazio_4x4);
    RUN_TEST(test_equivalencia_no_labirinto_vazio_8x4);
    RUN_TEST(test_equivalencia_no_labirinto_vazio_12x4);
    RUN_TEST(test_equivalencia_na_serpentina_12x4);
    RUN_TEST(test_equivalencia_8x4_com_longo_x);
    RUN_TEST(test_equivalencia_8x4_com_longo_y);
    RUN_TEST(test_equivalencia_12x4_com_longo_x);
    RUN_TEST(test_equivalencia_12x4_com_longo_y);
    RUN_TEST(test_equivalencia_em_labirintos_aleatorios_4x4);
    RUN_TEST(test_equivalencia_em_labirintos_aleatorios_8x4);
    RUN_TEST(test_equivalencia_em_labirintos_aleatorios_12x4);
    RUN_TEST(test_equivalencia_com_o_objetivo_cercado);
    return UNITY_END();
}
