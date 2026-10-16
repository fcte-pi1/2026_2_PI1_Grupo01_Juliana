/*
 * Tempo do flood fill no pior caso: 12x4 com a orientação indefinida (região
 * em "L") e paredes em serpentina. O caminho fica o mais longo possível e,
 * como a varredura de Harrison anda em x e y crescentes, cada passo para oeste
 * ou para o sul custa uma varredura a mais.
 *
 * Roda no PC (pio test -e native, tempo por clock_gettime) e na placa
 * (pio test -e esp32_sim -f test_desempenho_nav, tempo por micros()). O limite
 * de 5 ms é o da ESP32; no PC o teste só garante que nada piorou muito.
 */

#include <stdint.h>
#include <stdio.h>
#include <unity.h>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <time.h>
#endif

#include "nav.h"

#define EXECUCOES 100
#define LIMITE_US 5000

void setUp(void) {}
void tearDown(void) {}

static uint32_t agora_us(void)
{
#ifdef ARDUINO
    return (uint32_t)micros();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000);
#endif
}

/* Parede entre (x, y) e (x, y + 1), gravada nos dois lados. */
static void parede_ao_norte(nav_estado_t *nav, int x, int y)
{
    nav->mapa[x][y].paredes |= NAV_PAREDE_N;
    nav->mapa[x][y + 1].paredes |= NAV_PAREDE_S;
}

/*
 * Serpentina por linhas: entre cada linha y e a de cima há parede em toda a
 * largura, menos numa ponta, que alterna entre a última coluna e a coluna 0.
 * No braço de x (y até 3) a largura é 12; no braço de y, 4.
 */
static void montar_serpentina(nav_estado_t *nav)
{
    nav_iniciar(nav, NAV_TIPO_12X4);
    for (int y = 0; y < NAV_MAX_CELULAS - 1; y++) {
        int largura = y < 3 ? NAV_MAX_CELULAS : 4;
        int passagem = y % 2 == 0 ? largura - 1 : 0;
        for (int x = 0; x < largura; x++) {
            if (x != passagem) {
                parede_ao_norte(nav, x, y);
            }
        }
    }
    nav_recalcular(nav);
}

static void test_serpentina_tem_caminho_longo(void)
{
    nav_estado_t nav;
    montar_serpentina(&nav);
    TEST_ASSERT_EQUAL_INT(NAV_ORIENTACAO_INDEFINIDA, nav_orientacao(&nav));
    /* Da largada ao objetivo (11, 3): as linhas 0, 1 e 2 de ponta a ponta e as três subidas. */
    TEST_ASSERT_EQUAL_UINT8(3 * (NAV_MAX_CELULAS - 1) + 3, nav_distancia(&nav, 0, 0));
}

static void test_recalculo_no_pior_caso_fica_abaixo_de_5_ms(void)
{
    nav_estado_t nav;
    montar_serpentina(&nav);

    uint32_t soma = 0;
    uint32_t maximo = 0;
    for (int i = 0; i < EXECUCOES; i++) {
        nav.mapa_alterado = true;
        uint32_t inicio = agora_us();
        nav_recalcular(&nav);
        uint32_t duracao = agora_us() - inicio;
        soma += duracao;
        if (duracao > maximo) {
            maximo = duracao;
        }
    }

    char mensagem[80];
    snprintf(mensagem, sizeof mensagem, "nav_recalcular em %d execucoes: media %lu us, maximo %lu us",
             EXECUCOES, (unsigned long)(soma / EXECUCOES), (unsigned long)maximo);
    TEST_MESSAGE(mensagem);
    TEST_ASSERT_LESS_THAN_UINT32(LIMITE_US, maximo);
}

static int rodar_testes(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_serpentina_tem_caminho_longo);
    RUN_TEST(test_recalculo_no_pior_caso_fica_abaixo_de_5_ms);
    return UNITY_END();
}

#ifdef ARDUINO
void setup(void)
{
    delay(2000); /* dá tempo de o monitor serial abrir */
    rodar_testes();
}

void loop(void) {}
#else
int main(void)
{
    return rodar_testes();
}
#endif
