/*
 * A navegação de verdade (nav_passo) nos labirintos do FIRM-02, lidos de
 * simulador-navegacao/labirintos. Cada labirinto existe nas versões -dir e
 * -esq; no referencial do robô as duas têm de dar o mesmo trajeto (D5, D9).
 * Com 5% de leituras erradas, a corrida pode falhar, mas sempre termina e
 * nunca declara sucesso fora do objetivo.
 */

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "sim_corrida.h"
#include "sim_labirinto.h"

/* O pio test -e native roda o teste a partir de src/firmware. */
#define PASTA_LABIRINTOS "simulador-navegacao/labirintos/"
#define TAMANHO_ARQUIVO 4096
#define MAX_CELULAS 256
#define RUIDO_PCT 5
#define SEMENTES 20

static const char *const PARES[] = {
    "4x4-01",         "4x4-02",         "4x4-03",          "4x4-04",
    "8x4-lateral-01", "8x4-lateral-02", "8x4-frente-01",   "8x4-frente-02",
    "12x4-lateral-01", "12x4-lateral-02", "12x4-frente-01", "12x4-frente-02",
};
#define QTD_PARES (sizeof(PARES) / sizeof(PARES[0]))

typedef struct {
    int qtd;
    sim_celula_t celulas[MAX_CELULAS];
} trajeto_t;

void setUp(void) {}
void tearDown(void) {}

static void anotar_celula(void *ctx, const sim_celula_t *celula)
{
    trajeto_t *t = (trajeto_t *)ctx;
    if (t->qtd < MAX_CELULAS) {
        t->celulas[t->qtd] = *celula;
    }
    t->qtd++;
}

static void carregar(sim_labirinto_t *lab, const char *nome, const char *versao)
{
    static char texto[TAMANHO_ARQUIVO];
    char caminho[128];
    char erro[96] = {0};

    snprintf(caminho, sizeof(caminho), PASTA_LABIRINTOS "%s-%s.txt", nome, versao);
    FILE *f = fopen(caminho, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, caminho);
    size_t n = fread(texto, 1, sizeof(texto) - 1, f);
    fclose(f);
    texto[n] = '\0';

    TEST_ASSERT_TRUE_MESSAGE(sim_labirinto_carregar(lab, texto), caminho);
    TEST_ASSERT_TRUE_MESSAGE(sim_validar(lab, erro, sizeof(erro)), erro);
}

static sim_relatorio_t correr_com_ruido(const char *nome, const char *versao, trajeto_t *trajeto,
                                        uint8_t ruido_pct, uint32_t semente)
{
    sim_labirinto_t lab;
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.ruido_pct = ruido_pct;
    config.semente = semente;
    config.observador = anotar_celula;
    config.observador_ctx = trajeto;

    memset(trajeto, 0, sizeof(*trajeto));
    carregar(&lab, nome, versao);
    sim_correr(&lab, &config, &rel);
    return rel;
}

static sim_relatorio_t correr(const char *nome, const char *versao, trajeto_t *trajeto)
{
    return correr_com_ruido(nome, versao, trajeto, 0, 0);
}

static void test_todos_os_labirintos_terminam_em_sucesso(void)
{
    static trajeto_t trajeto;
    static const char *const VERSOES[] = {"dir", "esq"};

    for (size_t i = 0; i < QTD_PARES; i++) {
        for (size_t v = 0; v < 2; v++) {
            sim_relatorio_t rel = correr(PARES[i], VERSOES[v], &trajeto);
            TEST_ASSERT_EQUAL_STRING_MESSAGE(sim_resultado_nome(SIM_RESULTADO_SUCESSO),
                                             sim_resultado_nome(rel.resultado), PARES[i]);
        }
    }
}

static void test_versoes_espelhadas_dao_o_mesmo_trajeto(void)
{
    static trajeto_t dir;
    static trajeto_t esq;
    char mensagem[64];

    for (size_t i = 0; i < QTD_PARES; i++) {
        correr(PARES[i], "dir", &dir);
        correr(PARES[i], "esq", &esq);

        TEST_ASSERT_TRUE_MESSAGE(dir.qtd <= MAX_CELULAS, PARES[i]);
        TEST_ASSERT_EQUAL_INT_MESSAGE(dir.qtd, esq.qtd, PARES[i]);
        for (int c = 0; c < dir.qtd; c++) {
            snprintf(mensagem, sizeof(mensagem), "%s, celula %d", PARES[i], c);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(dir.celulas[c].x, esq.celulas[c].x, mensagem);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(dir.celulas[c].y, esq.celulas[c].y, mensagem);
        }
    }
}

static void test_com_ruido_a_corrida_termina_sem_sucesso_falso(void)
{
    static trajeto_t trajeto;
    static const char *const VERSOES[] = {"dir", "esq"};
    char mensagem[64];

    for (size_t i = 0; i < QTD_PARES; i++) {
        for (size_t v = 0; v < 2; v++) {
            for (uint32_t semente = 1; semente <= SEMENTES; semente++) {
                sim_relatorio_t rel = correr_com_ruido(PARES[i], VERSOES[v], &trajeto, RUIDO_PCT, semente);
                snprintf(mensagem, sizeof(mensagem), "%s-%s, semente %u", PARES[i], VERSOES[v],
                         (unsigned)semente);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(SIM_RESULTADO_LIMITE, rel.resultado, mensagem);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(SIM_RESULTADO_SUCESSO_FALSO, rel.resultado, mensagem);
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_todos_os_labirintos_terminam_em_sucesso);
    RUN_TEST(test_versoes_espelhadas_dao_o_mesmo_trajeto);
    RUN_TEST(test_com_ruido_a_corrida_termina_sem_sucesso_falso);
    return UNITY_END();
}
