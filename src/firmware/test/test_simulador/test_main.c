/*
 * Testes do simulador de navegação (FIRM-02). Enquanto o flood fill (FIRM-01)
 * não existe, a corrida é conferida com um dublê: um seguidor de parede que
 * sabe onde fica o objetivo.
 */

#include <string.h>
#include <unity.h>

#include "sim_corrida.h"
#include "sim_labirinto.h"
#include "sim_roteiro.h"

/* O labirinto_demo.h, com a largada marcada. */
static const char DEMO_DIR[] =
    "+---+---+---+---+\n"
    "|           |   |\n"
    "+   +---+   +   +\n"
    "|   |           |\n"
    "+   +   +---+   +\n"
    "|   |   |       |\n"
    "+   +---+   +---+\n"
    "| L |           |\n"
    "+---+---+---+---+\n";

/* O mesmo, espelhado: o labirinto fica à esquerda da largada. */
static const char DEMO_ESQ[] =
    "+---+---+---+---+\n"
    "|   |           |\n"
    "+   +   +---+   +\n"
    "|           |   |\n"
    "+   +---+   +   +\n"
    "|       |   |   |\n"
    "+---+   +---+   +\n"
    "|           | L |\n"
    "+---+---+---+---+\n";

/* 8x4 com o lado longo à frente da largada. */
static const char FRENTE_8X4[] =
    "+---+---+---+---+\n"
    "|               |\n"
    "+   +---+---+   +\n"
    "|   |       |   |\n"
    "+   +   +   +   +\n"
    "|   |   |   |   |\n"
    "+   +   +   +   +\n"
    "|   |   |   |   |\n"
    "+   +   +   +   +\n"
    "|   |   |   |   |\n"
    "+   +   +   +   +\n"
    "|   |   |   |   |\n"
    "+   +   +   +   +\n"
    "|   |   |   |   |\n"
    "+   +   +---+   +\n"
    "| L |   |       |\n"
    "+---+---+---+---+\n";

static sim_labirinto_t lab;
static char erro[96];

void setUp(void) { memset(erro, 0, sizeof(erro)); }
void tearDown(void) {}

/* ---- Dublês da navegação ----------------------------------------------- */

typedef struct {
    int x, y;
    nav_direcao_t rumo;
    int objetivo_x, objetivo_y;
    bool mao_esquerda; /* false: segue a parede da direita */
} seguidor_t;

static nav_acao_t seguir_parede(void *ctx, nav_leitura_t l)
{
    static const int DX[4] = {0, 1, 0, -1};
    static const int DY[4] = {1, 0, -1, 0};
    seguidor_t *s = (seguidor_t *)ctx;

    if (s->x == s->objetivo_x && s->y == s->objetivo_y) {
        return NAV_ACAO_SUCESSO;
    }
    bool lado_livre = s->mao_esquerda ? !l.esquerda : !l.direita;
    bool outro_livre = s->mao_esquerda ? !l.direita : !l.esquerda;
    nav_acao_t lado = s->mao_esquerda ? NAV_ACAO_ESQUERDA : NAV_ACAO_DIREITA;
    nav_acao_t outro = s->mao_esquerda ? NAV_ACAO_DIREITA : NAV_ACAO_ESQUERDA;

    nav_acao_t acao = lado_livre ? lado : !l.frente ? NAV_ACAO_FRENTE : outro_livre ? outro : NAV_ACAO_MEIA_VOLTA;

    /* O seguidor conta x positivo para o lado em que segue a parede. */
    s->rumo = nav_direcao_apos(s->rumo, acao);
    int dx = DX[s->rumo];
    s->x += s->mao_esquerda ? -dx : dx;
    s->y += DY[s->rumo];
    return acao;
}

static seguidor_t seguidor(int objetivo_x, int objetivo_y, bool mao_esquerda)
{
    seguidor_t s = {0, 0, NAV_NORTE, objetivo_x, objetivo_y, mao_esquerda};
    return s;
}

static nav_acao_t sempre(void *ctx, nav_leitura_t l)
{
    (void)l;
    return *(nav_acao_t *)ctx;
}

typedef struct {
    int chamadas;
    nav_leitura_t primeira;
} gravador_t;

static nav_acao_t gravar_e_parar(void *ctx, nav_leitura_t l)
{
    gravador_t *g = (gravador_t *)ctx;
    if (g->chamadas++ == 0) {
        g->primeira = l;
    }
    return NAV_ACAO_SEM_PASSO;
}

/* Anda reto e dá meia-volta em cada parede: nunca termina. */
static nav_acao_t quicar(void *ctx, nav_leitura_t l)
{
    (void)ctx;
    return l.frente ? NAV_ACAO_MEIA_VOLTA : NAV_ACAO_FRENTE;
}

static sim_relatorio_t correr(const char *texto, sim_decisor_t decisor, void *ctx)
{
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.decisor = decisor;
    config.decisor_ctx = ctx;

    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, texto));
    TEST_ASSERT_TRUE_MESSAGE(sim_validar(&lab, erro, sizeof(erro)), erro);
    sim_correr(&lab, &config, &rel);
    return rel;
}

/* ---- Validação ---------------------------------------------------------- */

static void test_aceita_labirinto_valido_e_le_a_largada(void)
{
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, DEMO_ESQ));
    TEST_ASSERT_TRUE_MESSAGE(sim_validar(&lab, erro, sizeof(erro)), erro);
    TEST_ASSERT_EQUAL_UINT8(3, lab.largada_x);
    TEST_ASSERT_EQUAL_UINT8(0, lab.largada_y);
    TEST_ASSERT_EQUAL_INT(SIM_TIPO_4X4, sim_tipo(&lab));
}

static void test_sem_marca_a_largada_fica_em_0_0(void)
{
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, "+---+\n|   |\n+   +\n|   |\n+---+\n"));
    TEST_ASSERT_EQUAL_UINT8(0, lab.largada_x);
    TEST_ASSERT_EQUAL_UINT8(0, lab.largada_y);
}

static void test_tipo_vale_nas_duas_orientacoes(void)
{
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, FRENTE_8X4));
    TEST_ASSERT_EQUAL_INT(SIM_TIPO_8X4, sim_tipo(&lab));
    TEST_ASSERT_TRUE_MESSAGE(sim_validar(&lab, erro, sizeof(erro)), erro);
    TEST_ASSERT_EQUAL_STRING("8x4", sim_tipo_nome(sim_tipo(&lab)));
}

static void test_rejeita_tamanho_fora_da_competicao(void)
{
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab,
        "+---+---+---+\n"
        "|           |\n"
        "+   +---+   +\n"
        "| L |       |\n"
        "+---+---+---+\n"));
    TEST_ASSERT_FALSE(sim_validar(&lab, erro, sizeof(erro)));
    TEST_ASSERT_NOT_NULL(strstr(erro, "tamanho"));
}

static void test_rejeita_borda_aberta(void)
{
    char texto[sizeof(DEMO_DIR)];
    memcpy(texto, DEMO_DIR, sizeof(texto));
    texto[18 + 16] = ' '; /* tira a parede leste da linha de cima */
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, texto));
    TEST_ASSERT_FALSE(sim_validar(&lab, erro, sizeof(erro)));
    TEST_ASSERT_NOT_NULL(strstr(erro, "borda"));
}

static void test_rejeita_largada_fora_do_canto_ou_sem_beco(void)
{
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab,
        "+---+---+---+---+\n|               |\n+   +   +   +   +\n|               |\n"
        "+   +   +   +   +\n|               |\n+   +   +   +   +\n|     L         |\n+---+---+---+---+\n"));
    TEST_ASSERT_FALSE(sim_validar(&lab, erro, sizeof(erro)));
    TEST_ASSERT_NOT_NULL(strstr(erro, "canto"));

    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab,
        "+---+---+---+---+\n|               |\n+   +   +   +   +\n|               |\n"
        "+   +   +   +   +\n|               |\n+   +   +   +   +\n| L             |\n+---+---+---+---+\n"));
    TEST_ASSERT_FALSE(sim_validar(&lab, erro, sizeof(erro)));
    TEST_ASSERT_NOT_NULL(strstr(erro, "saida"));
}

static void test_rejeita_objetivo_inalcancavel(void)
{
    /* O labirinto_demo.h antes do FIRM-02: (3, 3) ficava isolado. */
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab,
        "+---+---+---+---+\n"
        "|           |   |\n"
        "+   +---+   +   +\n"
        "|   |       |   |\n"
        "+   +   +---+   +\n"
        "|   |   |       |\n"
        "+   +---+   +---+\n"
        "|   |           |\n"
        "+---+---+---+---+\n"));
    TEST_ASSERT_FALSE(sim_validar(&lab, erro, sizeof(erro)));
    TEST_ASSERT_NOT_NULL(strstr(erro, "inalcancavel"));
}

/* ---- Corrida ------------------------------------------------------------ */

static void test_navegacao_real_e_chamada_por_padrao(void)
{
    /* Sem decisor, quem decide é nav_passo(). O esqueleto atual não anda. */
    sim_relatorio_t rel = correr(DEMO_DIR, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_SEM_CAMINHO, rel.resultado);
    TEST_ASSERT_EQUAL_UINT16(0, rel.celulas);
    TEST_ASSERT_EQUAL_UINT16(1, rel.distintas);
}

static void test_seguidor_de_parede_chega_ao_objetivo(void)
{
    seguidor_t s = seguidor(3, 3, false);
    sim_relatorio_t rel = correr(DEMO_DIR, seguir_parede, &s);

    /*
     * Seguindo a parede da direita, o robô entra nos becos (1,1), (1,0) e
     * (3,0) e volta de cada um com um giro de 180°:
     * (0,0) (0,1) (0,2) (0,3) (1,3) (2,3) (2,2) (1,2) (1,1) (1,2) (2,2) (3,2)
     * (3,1) (2,1) (2,0) (1,0) (2,0) (3,0) (2,0) (2,1) (3,1) (3,2) (3,3)
     */
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_SUCESSO, rel.resultado);
    TEST_ASSERT_EQUAL_UINT16(22, rel.celulas);
    TEST_ASSERT_EQUAL_UINT16(16, rel.distintas);
    TEST_ASSERT_EQUAL_UINT16(12, rel.giros_90);
    TEST_ASSERT_EQUAL_UINT16(3, rel.giros_180);
    TEST_ASSERT_EQUAL_UINT8(3, rel.x);
    TEST_ASSERT_EQUAL_UINT8(3, rel.y);
}

static void test_labirinto_espelhado_da_a_mesma_corrida(void)
{
    seguidor_t dir = seguidor(3, 3, false);
    seguidor_t esq = seguidor(3, 3, true);
    sim_relatorio_t a = correr(DEMO_DIR, seguir_parede, &dir);
    sim_relatorio_t b = correr(DEMO_ESQ, seguir_parede, &esq);

    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_SUCESSO, b.resultado);
    TEST_ASSERT_EQUAL_UINT16(a.celulas, b.celulas);
    TEST_ASSERT_EQUAL_UINT16(a.giros_90, b.giros_90);
    /* No referencial do robô, o objetivo é (3, 3) nos dois. */
    TEST_ASSERT_EQUAL_UINT8(3, b.x);
    TEST_ASSERT_EQUAL_UINT8(3, b.y);
}

static void test_lado_longo_a_frente_tem_objetivo_em_3_7(void)
{
    seguidor_t s = seguidor(3, 7, false);
    sim_relatorio_t rel = correr(FRENTE_8X4, seguir_parede, &s);
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_SUCESSO, rel.resultado);
    TEST_ASSERT_EQUAL_UINT8(3, rel.x);
    TEST_ASSERT_EQUAL_UINT8(7, rel.y);
}

static void test_andar_contra_a_parede_e_colisao(void)
{
    nav_acao_t frente = NAV_ACAO_FRENTE;
    sim_relatorio_t rel = correr(DEMO_DIR, sempre, &frente);
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_COLISAO, rel.resultado);
    TEST_ASSERT_EQUAL_UINT16(3, rel.celulas);
    TEST_ASSERT_EQUAL_UINT8(0, rel.x);
    TEST_ASSERT_EQUAL_UINT8(3, rel.y);
}

static void test_sucesso_fora_do_objetivo_e_falso(void)
{
    nav_acao_t sucesso = NAV_ACAO_SUCESSO;
    sim_relatorio_t rel = correr(DEMO_DIR, sempre, &sucesso);
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_SUCESSO_FALSO, rel.resultado);
}

static void test_para_no_limite_de_passos(void)
{
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.decisor = quicar;
    config.limite_passos = 10;

    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, DEMO_DIR));
    sim_correr(&lab, &config, &rel);
    TEST_ASSERT_EQUAL_INT(SIM_RESULTADO_LIMITE, rel.resultado);
    /* Sobe a coluna 0 até (0,3), desce até (0,0), sobe de novo: 3 + 3 + 3 + 1. */
    TEST_ASSERT_EQUAL_UINT16(10, rel.celulas);
    TEST_ASSERT_EQUAL_UINT16(4, rel.distintas);
    TEST_ASSERT_EQUAL_UINT16(3, rel.giros_180);
    TEST_ASSERT_EQUAL_UINT8(2, rel.y);
}

static void test_ruido_troca_as_leituras(void)
{
    gravador_t g = {0};
    sim_relatorio_t rel;
    sim_config_t config = {0};
    config.decisor = gravar_e_parar;
    config.decisor_ctx = &g;

    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, DEMO_DIR));
    sim_correr(&lab, &config, &rel);
    TEST_ASSERT_FALSE(g.primeira.frente);
    TEST_ASSERT_TRUE(g.primeira.esquerda);
    TEST_ASSERT_TRUE(g.primeira.direita);
    TEST_ASSERT_EQUAL_UINT16(0, rel.leituras_erradas);

    config.ruido_pct = 100;
    g.chamadas = 0;
    sim_correr(&lab, &config, &rel);
    TEST_ASSERT_TRUE(g.primeira.frente);
    TEST_ASSERT_FALSE(g.primeira.esquerda);
    TEST_ASSERT_FALSE(g.primeira.direita);
    TEST_ASSERT_EQUAL_UINT16(3, rel.leituras_erradas);
}

static void test_mesma_semente_mesma_corrida(void)
{
    sim_relatorio_t a, b;
    sim_config_t config = {0};
    seguidor_t s;
    config.decisor = seguir_parede;
    config.decisor_ctx = &s;
    config.ruido_pct = 20;
    config.semente = 42;

    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, DEMO_DIR));
    s = seguidor(3, 3, false);
    sim_correr(&lab, &config, &a);
    s = seguidor(3, 3, false);
    sim_correr(&lab, &config, &b);
    TEST_ASSERT_EQUAL_INT(a.resultado, b.resultado);
    TEST_ASSERT_EQUAL_UINT16(a.celulas, b.celulas);
    TEST_ASSERT_EQUAL_UINT16(a.leituras_erradas, b.leituras_erradas);
    TEST_ASSERT_TRUE(a.leituras_erradas > 0);
}

/* ---- Roteiro de telemetria --------------------------------------------- */

#define MAX_LINHAS 200
static char linhas[MAX_LINHAS][SIM_ROTEIRO_TAMANHO_LINHA];
static int qtd_linhas;

static void guardar_linha(void *ctx, const char *linha)
{
    (void)ctx;
    TEST_ASSERT_TRUE(qtd_linhas < MAX_LINHAS);
    strcpy(linhas[qtd_linhas++], linha);
}

static void gerar_roteiro(const char *texto, seguidor_t *s)
{
    sim_roteiro_t roteiro;
    sim_relatorio_t rel;
    sim_config_t config = {0};

    qtd_linhas = 0;
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, texto));
    sim_roteiro_iniciar(&roteiro, 7, sim_tipo(&lab), guardar_linha, NULL);
    config.decisor = seguir_parede;
    config.decisor_ctx = s;
    config.observador = sim_roteiro_celula;
    config.observador_ctx = &roteiro;
    sim_correr(&lab, &config, &rel);
    sim_roteiro_finalizar(&roteiro, &rel);
}

static int procurar(const char *trecho, int a_partir_de)
{
    for (int i = a_partir_de; i < qtd_linhas; i++) {
        if (strstr(linhas[i], trecho)) {
            return i;
        }
    }
    return -1;
}

static void test_roteiro_segue_o_formato_do_contrato(void)
{
    seguidor_t s = seguidor(3, 3, false);
    gerar_roteiro(DEMO_DIR, &s);

    /* Mesmas chaves e mesma ordem dos exemplos do ARQ-01. */
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"boot\":7,\"seq\":0,\"t_ms\":250,\"tipo\":\"hc_item\","
        "\"componente\":\"bateria\",\"aprovado\":true,\"valor\":8100}\n", linhas[0]);
    TEST_ASSERT_TRUE(procurar("\"componente\":\"motor_esquerdo\",\"aprovado\":true,\"valor\":null}\n", 0) > 0);
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"hc_resultado\",\"aprovado\":true,\"tipo_dip\":\"4x4\",\"inicio\":\"nova\"}\n", 0) > 0);
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"tel\",\"estado\":\"health-check\",\"x\":0,\"y\":0,\"rumo\":\"N\"", 0) > 0);

    /* Largada: paredes ao sul, leste e oeste = 2 + 4 + 8. */
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"passo\",\"x\":0,\"y\":0,\"paredes\":14}\n", 0) > 0);
    /* Objetivo (3, 3): paredes ao norte, leste e oeste = 1 + 4 + 8. */
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"passo\",\"x\":3,\"y\":3,\"paredes\":13}\n", 0) > 0);
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"tel\",\"estado\":\"running\",\"x\":0,\"y\":3,\"rumo\":\"L\"", 0) > 0);

    TEST_ASSERT_NOT_NULL(strstr(linhas[qtd_linhas - 2], "\"tipo\":\"sucesso\",\"x\":3,\"y\":3}\n"));
    TEST_ASSERT_NOT_NULL(strstr(linhas[qtd_linhas - 1], "\"estado\":\"success\""));
}

static void test_roteiro_numera_e_ordena_as_mensagens(void)
{
    seguidor_t s = seguidor(3, 3, false);
    gerar_roteiro(DEMO_DIR, &s);

    unsigned long t_anterior = 0;
    for (int i = 0; i < qtd_linhas; i++) {
        char esperado[32];
        unsigned long t;
        snprintf(esperado, sizeof(esperado), "\"seq\":%d,", i);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(linhas[i], esperado), linhas[i]);
        TEST_ASSERT_TRUE(strlen(linhas[i]) < SIM_ROTEIRO_TAMANHO_LINHA);
        TEST_ASSERT_EQUAL_CHAR('\n', linhas[i][strlen(linhas[i]) - 1]);
        TEST_ASSERT_EQUAL_INT(1, sscanf(strstr(linhas[i], "\"t_ms\":"), "\"t_ms\":%lu", &t));
        TEST_ASSERT_TRUE(t >= t_anterior);
        t_anterior = t;
    }
}

static void test_roteiro_usa_o_referencial_do_robo_e_o_eixo_longo(void)
{
    /* Espelhado: o robô anda para o oeste do desenho, mas x continua positivo. */
    seguidor_t s = seguidor(3, 3, true);
    gerar_roteiro(DEMO_ESQ, &s);
    TEST_ASSERT_TRUE(procurar("\"tipo\":\"passo\",\"x\":3,\"y\":3,", 0) > 0);
    TEST_ASSERT_TRUE(procurar("\"eixo_longo\":null}", 0) > 0);
    TEST_ASSERT_EQUAL_INT(-1, procurar("\"eixo_longo\":\"", 0));

    s = seguidor(3, 7, false);
    gerar_roteiro(FRENTE_8X4, &s);
    int primeira_y4 = procurar("\"tipo\":\"passo\",\"x\":0,\"y\":4,", 0);
    TEST_ASSERT_TRUE(primeira_y4 > 0);
    /* O eixo longo só aparece depois do passo em y = 4. */
    TEST_ASSERT_TRUE(procurar("\"eixo_longo\":\"y\"}", 0) > primeira_y4);
    TEST_ASSERT_TRUE(procurar("\"tipo_dip\":\"8x4\"", 0) > 0);
}

static void test_roteiro_termina_com_falha_quando_bate(void)
{
    sim_roteiro_t roteiro;
    sim_relatorio_t rel;
    sim_config_t config = {0};
    nav_acao_t frente = NAV_ACAO_FRENTE;

    qtd_linhas = 0;
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab, DEMO_DIR));
    sim_roteiro_iniciar(&roteiro, 1, sim_tipo(&lab), guardar_linha, NULL);
    config.decisor = sempre;
    config.decisor_ctx = &frente;
    config.observador = sim_roteiro_celula;
    config.observador_ctx = &roteiro;
    sim_correr(&lab, &config, &rel);
    sim_roteiro_finalizar(&roteiro, &rel);

    TEST_ASSERT_NOT_NULL(strstr(linhas[qtd_linhas - 2],
        "\"tipo\":\"falha\",\"motivo\":\"collision\",\"origem\":\"automatica\",\"x\":0,\"y\":3,\"componente\":null}\n"));
    TEST_ASSERT_NOT_NULL(strstr(linhas[qtd_linhas - 1], "\"estado\":\"failed\""));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_aceita_labirinto_valido_e_le_a_largada);
    RUN_TEST(test_sem_marca_a_largada_fica_em_0_0);
    RUN_TEST(test_tipo_vale_nas_duas_orientacoes);
    RUN_TEST(test_rejeita_tamanho_fora_da_competicao);
    RUN_TEST(test_rejeita_borda_aberta);
    RUN_TEST(test_rejeita_largada_fora_do_canto_ou_sem_beco);
    RUN_TEST(test_rejeita_objetivo_inalcancavel);
    RUN_TEST(test_navegacao_real_e_chamada_por_padrao);
    RUN_TEST(test_seguidor_de_parede_chega_ao_objetivo);
    RUN_TEST(test_labirinto_espelhado_da_a_mesma_corrida);
    RUN_TEST(test_lado_longo_a_frente_tem_objetivo_em_3_7);
    RUN_TEST(test_andar_contra_a_parede_e_colisao);
    RUN_TEST(test_sucesso_fora_do_objetivo_e_falso);
    RUN_TEST(test_para_no_limite_de_passos);
    RUN_TEST(test_ruido_troca_as_leituras);
    RUN_TEST(test_mesma_semente_mesma_corrida);
    RUN_TEST(test_roteiro_segue_o_formato_do_contrato);
    RUN_TEST(test_roteiro_numera_e_ordena_as_mensagens);
    RUN_TEST(test_roteiro_usa_o_referencial_do_robo_e_o_eixo_longo);
    RUN_TEST(test_roteiro_termina_com_falha_quando_bate);
    return UNITY_END();
}
