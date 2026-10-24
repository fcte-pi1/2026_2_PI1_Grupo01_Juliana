/*
 * NVS dentro do laço do carrinho, com a HAL simulada e o labirinto de
 * demonstração (o mesmo de src/app). Não usa leituras inventadas: as paredes
 * saem dos ToF simulados, no mesmo ritmo da tarefa de navegação (4 leituras
 * de 50 ms, depois uma decisão).
 */

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "config/robo.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "memoria.h"
#include "sensores.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"
#include "../../src/app/labirinto_demo.h"

#define PERIODO_MS         50
#define CICLOS_POR_DECISAO 4
#define PASSOS_ANTES       6
#define PASSOS_DEPOIS      4

static nav_estado_t ao_vivo;
static nav_estado_t retomada;
static contrato_estado_t estado_vivo;
static contrato_estado_t estado_retomada;
static int8_t gravada_x;
static int8_t gravada_y;
static bool terminou;
static nav_acao_t acao;

/* Igual ao boot de app_iniciar + o começo da tarefa de navegação. */
static mem_inicio_t boot(nav_estado_t *nav, contrato_estado_t *estado, int8_t *x, int8_t *y,
                         bool *fim)
{
    sim_mundo_iniciar(LABIRINTO_DEMO);
    (void)hal_tof_iniciar();
    sensores_iniciar();
    (void)hal_nvs_iniciar();
    mem_inicio_t inicio = mem_carregar(nav, estado, ROBO_TIPO_LABIRINTO);
    *x = nav->x;
    *y = nav->y;
    *fim = nav->terminou;
    return inicio;
}

/* Quatro amostras, como a tarefa, e a decisão que grava a NVS. */
static nav_leitura_t ler_celula(void)
{
    for (int i = 0; i < CICLOS_POR_DECISAO; i++) {
        sim_mundo_avancar(PERIODO_MS);
        sensores_ler();
    }
    paredes_t paredes = sensores_paredes();
    nav_leitura_t leitura = {paredes.frente, paredes.esquerda, paredes.direita};
    return leitura;
}

static void decidir(nav_estado_t *nav, contrato_estado_t *estado, int8_t *x, int8_t *y, bool *fim,
                    nav_leitura_t leitura)
{
    acao = mem_aplicar_leitura(nav, estado, x, y, fim, leitura);
}

static void estragar_mapa(void)
{
    static uint8_t bloco[2048];
    size_t n = hal_nvs_sim_ler_bruto(MEM_CHAVE_MAPA, bloco, sizeof(bloco));
    TEST_ASSERT_TRUE(n > 100);
    bloco[100] ^= 0x5A;
    TEST_ASSERT_TRUE(hal_nvs_gravar(MEM_CHAVE_MAPA, bloco, n));
}

void setUp(void) { hal_nvs_sim_apagar_tudo(); }

void tearDown(void) {}

static void test_reset_no_meio_do_labirinto_demo_volta_na_mesma_celula(void)
{
    TEST_ASSERT_EQUAL_INT(MEM_NOVA, boot(&ao_vivo, &estado_vivo, &gravada_x, &gravada_y, &terminou));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, estado_vivo);

    for (int i = 0; i < PASSOS_ANTES; i++) {
        decidir(&ao_vivo, &estado_vivo, &gravada_x, &gravada_y, &terminou, ler_celula());
    }
    TEST_ASSERT_TRUE_MESSAGE(ao_vivo.x != 0 || ao_vivo.y != 0, "a corrida nao saiu da largada");

    nav_estado_t copia = ao_vivo;
    contrato_estado_t estado_copia = estado_vivo;
    int8_t copia_x = gravada_x;
    int8_t copia_y = gravada_y;
    bool copia_fim = terminou;

    /* Reset: a NVS simulada fica; o mundo e os filtros recomeçam, como o firmware. */
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA,
                          boot(&retomada, &estado_retomada, &gravada_x, &gravada_y, &terminou));
    TEST_ASSERT_EQUAL_INT(estado_copia, estado_retomada);
    TEST_ASSERT_EQUAL_INT8(copia.x, retomada.x);
    TEST_ASSERT_EQUAL_INT8(copia.y, retomada.y);
    TEST_ASSERT_EQUAL_INT(copia.direcao, retomada.direcao);
    TEST_ASSERT_EQUAL_MEMORY(&copia, &retomada, sizeof(copia));

    printf("retomada na celula (%d,%d), rumo %d, estado %d\n", retomada.x, retomada.y,
           (int)retomada.direcao, (int)estado_retomada);

    for (int i = 0; i < PASSOS_DEPOIS; i++) {
        nav_leitura_t leitura = ler_celula();
        decidir(&copia, &estado_copia, &copia_x, &copia_y, &copia_fim, leitura);
        nav_acao_t acao_copia = acao;
        decidir(&retomada, &estado_retomada, &gravada_x, &gravada_y, &terminou, leitura);
        TEST_ASSERT_EQUAL_INT(acao_copia, acao);
    }
    TEST_ASSERT_EQUAL_INT(estado_copia, estado_retomada);
    TEST_ASSERT_EQUAL_MEMORY(&copia, &retomada, sizeof(copia));
}

static void test_crc_estragado_no_fluxo_volta_ao_mapa_vazio(void)
{
    boot(&ao_vivo, &estado_vivo, &gravada_x, &gravada_y, &terminou);
    for (int i = 0; i < PASSOS_ANTES; i++) {
        decidir(&ao_vivo, &estado_vivo, &gravada_x, &gravada_y, &terminou, ler_celula());
    }
    estragar_mapa();

    TEST_ASSERT_EQUAL_INT(MEM_SO_TIPOS,
                          boot(&retomada, &estado_retomada, &gravada_x, &gravada_y, &terminou));
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, estado_retomada);
    TEST_ASSERT_EQUAL_INT8(0, retomada.x);
    TEST_ASSERT_EQUAL_INT8(0, retomada.y);
    TEST_ASSERT_EQUAL_INT(NAV_NORTE, retomada.direcao);

    nav_estado_t vazio;
    nav_iniciar(&vazio, ROBO_TIPO_LABIRINTO);
    vazio.orientacao = ao_vivo.orientacao;
    vazio.lado = ao_vivo.lado;
    vazio.lado_provisorio = ao_vivo.lado_provisorio;
    vazio.lado_y = ao_vivo.lado_y;
    vazio.lado_duas_abertas = ao_vivo.lado_duas_abertas;
    vazio.desfazeres = ao_vivo.desfazeres;
    nav_recalcular(&vazio);
    TEST_ASSERT_EQUAL_MEMORY(&vazio, &retomada, sizeof(vazio));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_no_meio_do_labirinto_demo_volta_na_mesma_celula);
    RUN_TEST(test_crc_estragado_no_fluxo_volta_ao_mapa_vazio);
    return UNITY_END();
}
