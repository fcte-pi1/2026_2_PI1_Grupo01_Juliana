/*
 * A navegação de verdade (nav_passo) nos labirintos do FIRM-02, lidos de
 * simulador-navegacao/labirintos. Cada labirinto existe nas versões -dir e
 * -esq; no referencial do robô as duas têm de dar o mesmo trajeto (D5, D9).
 * Com leituras erradas e a conferência da frente de 3 amostras, as metas da
 * D18: todas em sucesso com 1%, pelo menos 95% com 5%, e nenhuma corrida em
 * limite ou sucesso_falso. Copiar o estado com memcpy no
 * meio da corrida e seguir com a cópia não muda nada (retomada, FIRM-08).
 * O trajeto anotado vai da largada ao objetivo, uma célula vizinha por vez
 * (caso 4.12 do 7.4, US39 e US41).
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "sim_corrida.h"
#include "sim_labirinto.h"

/* O pio test -e native roda o teste a partir de src/firmware. */
#define PASTA_LABIRINTOS "simulador-navegacao/labirintos/"
#define TAMANHO_ARQUIVO 4096
#define MAX_CELULAS 256
#define RUIDO_PCT 5
#define SEMENTES 50
#define AMOSTRAS_FRENTE 3

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

static sim_relatorio_t correr_com_conferencia(const char *nome, const char *versao, trajeto_t *trajeto,
                                              uint8_t ruido_pct, uint32_t semente, uint8_t amostras)
{
    sim_labirinto_t lab;
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.ruido_pct = ruido_pct;
    config.semente = semente;
    config.amostras_frente = amostras;
    config.observador = anotar_celula;
    config.observador_ctx = trajeto;

    memset(trajeto, 0, sizeof(*trajeto));
    carregar(&lab, nome, versao);
    sim_correr(&lab, &config, &rel);
    return rel;
}

static sim_relatorio_t correr_com_ruido(const char *nome, const char *versao, trajeto_t *trajeto,
                                        uint8_t ruido_pct, uint32_t semente)
{
    return correr_com_conferencia(nome, versao, trajeto, ruido_pct, semente, 0);
}

static sim_relatorio_t correr(const char *nome, const char *versao, trajeto_t *trajeto)
{
    return correr_com_ruido(nome, versao, trajeto, 0, 0);
}

/* Decisor que, no passo "parada", copia o estado e segue com a cópia. */
typedef struct {
    nav_estado_t original;
    nav_estado_t copia;
    nav_estado_t *atual;
    int passo;
    int parada;
} retomada_t;

static nav_acao_t decidir_com_retomada(void *ctx, nav_leitura_t leitura)
{
    retomada_t *r = (retomada_t *)ctx;
    if (r->passo == r->parada) {
        memcpy(&r->copia, &r->original, sizeof(r->copia));
        memset(&r->original, 0xAA, sizeof(r->original)); /* o original não pode mais ser usado */
        r->atual = &r->copia;
    }
    r->passo++;
    return nav_passo(r->atual, leitura);
}

static nav_tipo_t tipo_da_navegacao(sim_tipo_t tipo)
{
    switch (tipo) {
    case SIM_TIPO_8X4:
        return NAV_TIPO_8X4;
    case SIM_TIPO_12X4:
        return NAV_TIPO_12X4;
    default:
        return NAV_TIPO_4X4;
    }
}

static sim_relatorio_t correr_com_retomada(const char *nome, trajeto_t *trajeto, uint8_t ruido_pct,
                                           int parada)
{
    static retomada_t retomada;
    sim_labirinto_t lab;
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.ruido_pct = ruido_pct;
    config.semente = 1;
    config.decisor = decidir_com_retomada;
    config.decisor_ctx = &retomada;
    config.observador = anotar_celula;
    config.observador_ctx = trajeto;

    memset(trajeto, 0, sizeof(*trajeto));
    carregar(&lab, nome, "dir");
    nav_iniciar(&retomada.original, tipo_da_navegacao(sim_tipo(&lab)));
    retomada.atual = &retomada.original;
    retomada.passo = 0;
    retomada.parada = parada;
    sim_correr(&lab, &config, &rel);
    return rel;
}

static void test_todos_os_labirintos_terminam_em_sucesso(void)
{
    static trajeto_t trajeto;
    static const char *const VERSOES[] = {"dir", "esq"};
    int sucessos = 0;
    int sucessos_falsos = 0;
    char mensagem[96];

    for (size_t i = 0; i < QTD_PARES; i++) {
        for (size_t v = 0; v < 2; v++) {
            sim_relatorio_t rel = correr(PARES[i], VERSOES[v], &trajeto);
            sucessos += rel.resultado == SIM_RESULTADO_SUCESSO;
            sucessos_falsos += rel.resultado == SIM_RESULTADO_SUCESSO_FALSO;
            TEST_ASSERT_EQUAL_STRING_MESSAGE(sim_resultado_nome(SIM_RESULTADO_SUCESSO),
                                             sim_resultado_nome(rel.resultado), PARES[i]);
        }
    }

    snprintf(mensagem, sizeof(mensagem), "%d labirintos (4x4, 8x4 e 12x4): %d em sucesso, %d sucesso_falso",
             (int)(QTD_PARES * 2), sucessos, sucessos_falsos);
    TEST_MESSAGE(mensagem);
}

/*
 * US41: a lista de células reconstrói o trajeto. Começa na largada (0, 0),
 * cada célula é vizinha da anterior e a última é o canto oposto, onde o
 * robô declarou o sucesso (US39).
 */
static void test_trajeto_vai_da_largada_ao_objetivo_por_celulas_vizinhas(void)
{
    static trajeto_t trajeto;
    static const char *const VERSOES[] = {"dir", "esq"};
    char mensagem[64];

    for (size_t i = 0; i < QTD_PARES; i++) {
        for (size_t v = 0; v < 2; v++) {
            sim_labirinto_t lab;
            carregar(&lab, PARES[i], VERSOES[v]);
            sim_relatorio_t rel = correr(PARES[i], VERSOES[v], &trajeto);
            snprintf(mensagem, sizeof(mensagem), "%s-%s", PARES[i], VERSOES[v]);

            TEST_ASSERT_TRUE_MESSAGE(trajeto.qtd <= MAX_CELULAS, mensagem);
            TEST_ASSERT_EQUAL_INT_MESSAGE(rel.celulas + 1, trajeto.qtd, mensagem);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, trajeto.celulas[0].x, mensagem);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, trajeto.celulas[0].y, mensagem);

            bool visitada[SIM_LAB_MAX][SIM_LAB_MAX] = {{false}};
            int distintas = 0;
            for (int c = 0; c < trajeto.qtd; c++) {
                const sim_celula_t *atual = &trajeto.celulas[c];
                if (!visitada[atual->x][atual->y]) {
                    visitada[atual->x][atual->y] = true;
                    distintas++;
                }
                if (c == 0) {
                    continue;
                }
                const sim_celula_t *anterior = &trajeto.celulas[c - 1];
                int passo = abs(atual->x - anterior->x) + abs(atual->y - anterior->y);
                snprintf(mensagem, sizeof(mensagem), "%s-%s, celula %d", PARES[i], VERSOES[v], c);
                TEST_ASSERT_EQUAL_INT_MESSAGE(1, passo, mensagem);
            }
            snprintf(mensagem, sizeof(mensagem), "%s-%s", PARES[i], VERSOES[v]);
            TEST_ASSERT_EQUAL_INT_MESSAGE(rel.distintas, distintas, mensagem);

            /* No referencial do robô o objetivo é (L - 1, A - 1), com L e A o tamanho do labirinto em algum giro. */
            const sim_celula_t *ultima = &trajeto.celulas[trajeto.qtd - 1];
            int lx = ultima->x + 1;
            int ly = ultima->y + 1;
            bool canto_oposto = (lx == lab.largura && ly == lab.altura) || (lx == lab.altura && ly == lab.largura);
            TEST_ASSERT_TRUE_MESSAGE(canto_oposto, mensagem);
            TEST_ASSERT_EQUAL_INT_MESSAGE(NAV_ACAO_SUCESSO, ultima->acao, mensagem);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(rel.x, ultima->x, mensagem);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(rel.y, ultima->y, mensagem);
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

/* Corre os 24 labirintos com as sementes 1 a SEMENTES e devolve quantas deram sucesso. */
static int sucessos_com_ruido(uint8_t ruido_pct)
{
    static trajeto_t trajeto;
    static const char *const VERSOES[] = {"dir", "esq"};
    char mensagem[64];
    int sucessos = 0;

    for (size_t i = 0; i < QTD_PARES; i++) {
        for (size_t v = 0; v < 2; v++) {
            for (uint32_t semente = 1; semente <= SEMENTES; semente++) {
                sim_relatorio_t rel = correr_com_conferencia(PARES[i], VERSOES[v], &trajeto, ruido_pct,
                                                             semente, AMOSTRAS_FRENTE);
                snprintf(mensagem, sizeof(mensagem), "%s-%s, ruido %u, semente %u", PARES[i], VERSOES[v],
                         (unsigned)ruido_pct, (unsigned)semente);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(SIM_RESULTADO_LIMITE, rel.resultado, mensagem);
                TEST_ASSERT_NOT_EQUAL_MESSAGE(SIM_RESULTADO_SUCESSO_FALSO, rel.resultado, mensagem);
                if (rel.resultado == SIM_RESULTADO_SUCESSO) {
                    sucessos++;
                }
            }
        }
    }
    return sucessos;
}

static void conferir_meta(uint8_t ruido_pct, int meta_pct)
{
    const int total = (int)(QTD_PARES * 2 * SEMENTES);
    int sucessos = sucessos_com_ruido(ruido_pct);
    char mensagem[96];

    snprintf(mensagem, sizeof(mensagem), "com %u%% de ruido: %d de %d em sucesso (%.2f%%), meta %d%%",
             (unsigned)ruido_pct, sucessos, total, 100.0 * sucessos / total, meta_pct);
    TEST_MESSAGE(mensagem);
    TEST_ASSERT_TRUE_MESSAGE(sucessos * 100 >= meta_pct * total, mensagem);
}

static void test_com_1_por_cento_de_ruido_todas_dao_sucesso(void)
{
    conferir_meta(1, 100);
}

static void test_com_5_por_cento_de_ruido_pelo_menos_95_por_cento_dao_sucesso(void)
{
    conferir_meta(RUIDO_PCT, 95);
}

static void test_estado_copiado_no_meio_da_corrida_da_o_mesmo_trajeto(void)
{
    static const char *const UM_DE_CADA_TIPO[] = {"4x4-02", "8x4-frente-01", "12x4-lateral-01"};
    static const uint8_t RUIDOS[] = {0, RUIDO_PCT};
    static trajeto_t inteiro;
    static trajeto_t retomado;
    char mensagem[64];

    for (size_t i = 0; i < 3; i++) {
        for (size_t r = 0; r < 2; r++) {
            sim_relatorio_t esperado = correr_com_ruido(UM_DE_CADA_TIPO[i], "dir", &inteiro, RUIDOS[r], 1);
            TEST_ASSERT_TRUE_MESSAGE(inteiro.qtd <= MAX_CELULAS, UM_DE_CADA_TIPO[i]);

            for (int parada = 0; parada < inteiro.qtd; parada++) {
                sim_relatorio_t rel = correr_com_retomada(UM_DE_CADA_TIPO[i], &retomado, RUIDOS[r], parada);
                snprintf(mensagem, sizeof(mensagem), "%s, ruido %u, parada no passo %d",
                         UM_DE_CADA_TIPO[i], (unsigned)RUIDOS[r], parada);
                TEST_ASSERT_EQUAL_INT_MESSAGE(esperado.resultado, rel.resultado, mensagem);
                TEST_ASSERT_EQUAL_INT_MESSAGE(inteiro.qtd, retomado.qtd, mensagem);
                TEST_ASSERT_EQUAL_MEMORY_MESSAGE(inteiro.celulas, retomado.celulas,
                                                 sizeof(sim_celula_t) * (size_t)inteiro.qtd, mensagem);
                TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&esperado, &rel, sizeof(rel), mensagem);
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_todos_os_labirintos_terminam_em_sucesso);
    RUN_TEST(test_trajeto_vai_da_largada_ao_objetivo_por_celulas_vizinhas);
    RUN_TEST(test_versoes_espelhadas_dao_o_mesmo_trajeto);
    RUN_TEST(test_com_1_por_cento_de_ruido_todas_dao_sucesso);
    RUN_TEST(test_com_5_por_cento_de_ruido_pelo_menos_95_por_cento_dao_sucesso);
    RUN_TEST(test_estado_copiado_no_meio_da_corrida_da_o_mesmo_trajeto);
    return UNITY_END();
}
