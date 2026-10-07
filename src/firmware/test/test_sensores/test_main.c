/*
 * Testes da lib sensores (FIRM-03, #129): filtro, detecção de parede,
 * autoteste e sequência XSHUT.
 *
 * Os 50 casos sintéticos exercitam a detecção sem o mundo simulado; os casos
 * de ponta a ponta usam a HAL simulada com ruído de semente fixa.
 */

#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "config/pinos.h"
#include "config/robo.h"
#include "hal_tof.h"
#include "paredes.h"
#include "sensores.h"
#include "sim_mundo.h"
#include "tof_autoteste.h"
#include "tof_enderecos.h"
#include "tof_filtro.h"

#define PI_F 3.14159265f

/*
 * (0, 0) tem parede à esquerda (borda) e à direita, e a frente aberta até a
 * borda norte de (0, 1). (2, 0) não tem parede de nenhum lado nem na frente.
 */
static const char LABIRINTO_4X2[] =
    "+---+---+---+---+\n"
    "|   |           |\n"
    "+   +   +   +---+\n"
    "|   |           |\n"
    "+---+---+---+---+\n";

#define X_CELULA_ABERTA (CELULA_MM * 2.5f)

void setUp(void)
{
    sim_mundo_iniciar(LABIRINTO_4X2);
    sensores_iniciar();
}

void tearDown(void) {}

/* ---------------------------------------------------------------- filtro */

static void test_mediana_de_3_5_e_par(void)
{
    const uint16_t tres[] = {300, 40, 90};
    const uint16_t cinco[] = {5, 1, 4, 2, 3};
    const uint16_t dois[] = {100, 200};
    TEST_ASSERT_EQUAL_UINT16(90, tof_mediana(tres, 3));
    TEST_ASSERT_EQUAL_UINT16(3, tof_mediana(cinco, 5));
    TEST_ASSERT_EQUAL_UINT16(150, tof_mediana(dois, 2));
    TEST_ASSERT_EQUAL_UINT16(300, tres[0]); /* não altera a entrada */
    TEST_ASSERT_EQUAL_UINT16(0, tof_mediana(tres, 0));
    TEST_ASSERT_EQUAL_UINT16(40, tof_mediana(tres + 1, 1));
}

static void test_nada_a_vista_satura_no_alcance(void)
{
    TEST_ASSERT_EQUAL_UINT16(TOF_ALCANCE_MAX_MM, tof_saturar(8190));
    TEST_ASSERT_EQUAL_UINT16(TOF_ALCANCE_MAX_MM, tof_saturar(8191));
    TEST_ASSERT_EQUAL_UINT16(77, tof_saturar(77));

    tof_filtro_t f;
    tof_filtro_iniciar(&f);
    tof_filtro_inserir(&f, true, 8190);
    TEST_ASSERT_TRUE(tof_filtro_valido(&f));
    TEST_ASSERT_EQUAL_UINT16(TOF_ALCANCE_MAX_MM, tof_filtro_mediana(&f));
}

static void test_filtro_tolera_duas_falhas_e_descarta_na_terceira(void)
{
    tof_filtro_t f;
    tof_filtro_iniciar(&f);
    TEST_ASSERT_FALSE(tof_filtro_valido(&f)); /* sem leitura ainda */

    tof_filtro_inserir(&f, true, 77);
    tof_filtro_inserir(&f, false, 0);
    tof_filtro_inserir(&f, false, 0);
    TEST_ASSERT_TRUE(tof_filtro_valido(&f));
    tof_filtro_inserir(&f, true, 77); /* volta: zera a contagem */
    tof_filtro_inserir(&f, false, 0);
    tof_filtro_inserir(&f, false, 0);
    TEST_ASSERT_FALSE(f.falhou);

    tof_filtro_inserir(&f, false, 0);
    TEST_ASSERT_TRUE(f.falhou);
    TEST_ASSERT_FALSE(tof_filtro_valido(&f));

    /* Falho até reiniciar, mesmo que volte a responder. */
    tof_filtro_inserir(&f, true, 77);
    TEST_ASSERT_FALSE(tof_filtro_valido(&f));
    tof_filtro_iniciar(&f);
    tof_filtro_inserir(&f, true, 77);
    TEST_ASSERT_TRUE(tof_filtro_valido(&f));
}

static void test_janela_da_mediana_desliza(void)
{
    tof_filtro_t f;
    tof_filtro_iniciar(&f);
    tof_filtro_inserir(&f, true, 300);
    tof_filtro_inserir(&f, true, 300);
    tof_filtro_inserir(&f, true, 300);
    tof_filtro_inserir(&f, true, 50);
    TEST_ASSERT_EQUAL_UINT16(300, tof_filtro_mediana(&f)); /* 300, 300, 50 */
    tof_filtro_inserir(&f, true, 50);
    TEST_ASSERT_EQUAL_UINT16(50, tof_filtro_mediana(&f)); /* 300, 50, 50 */
    TEST_ASSERT_EQUAL_UINT8(TOF_MEDIANA_AMOSTRAS, f.quantidade);
}

static void test_histerese(void)
{
    TEST_ASSERT_TRUE(paredes_histerese(false, 119, 120, 150));
    TEST_ASSERT_FALSE(paredes_histerese(false, 120, 120, 150));
    TEST_ASSERT_FALSE(paredes_histerese(false, 135, 120, 150)); /* faixa: mantém */
    TEST_ASSERT_TRUE(paredes_histerese(true, 135, 120, 150));
    TEST_ASSERT_TRUE(paredes_histerese(true, 150, 120, 150));
    TEST_ASSERT_FALSE(paredes_histerese(true, 151, 120, 150));
}

/* ------------------------------------------------------ 50 casos sintéticos */

#define SEM     0xFFFF /* leitura sem resposta */
#define NADA    0      /* fim da sequência / sensor não usado */
#define MAX_SEQ 9

typedef enum { FRENTE, ESQUERDA, DIREITA } lado_t;

typedef struct {
    const char *descricao;
    lado_t lado;
    uint16_t a[MAX_SEQ]; /* frontal esquerdo, ou o lateral */
    uint16_t b[MAX_SEQ]; /* frontal direito (só na frente) */
    bool esperado;
} caso_t;

static const caso_t CASOS[] = {
    /* Frente presente */
    {"frente 44", FRENTE, {44, 44, 44, 44, 44}, {44, 44, 44, 44, 44}, true},
    {"frente 60", FRENTE, {60, 60, 60, 60, 60}, {60, 60, 60, 60, 60}, true},
    {"frente 100", FRENTE, {100, 100, 100, 100, 100}, {100, 100, 100, 100, 100}, true},
    {"frente 119", FRENTE, {119, 119, 119}, {119, 119, 119}, true},
    /* Frente ausente */
    {"frente 224", FRENTE, {224, 224, 224, 224, 224}, {224, 224, 224, 224, 224}, false},
    {"frente 1200", FRENTE, {1200, 1200, 1200}, {1200, 1200, 1200}, false},
    {"frente 8190", FRENTE, {8190, 8190, 8190}, {8190, 8190, 8190}, false},
    {"frente 400", FRENTE, {400, 400, 400}, {400, 400, 400}, false},
    /* Frente com ruído de ±15 mm */
    {"frente 44 ruido", FRENTE, {30, 58, 40, 52, 35}, {50, 29, 59, 44, 38}, true},
    {"frente 224 ruido", FRENTE, {210, 238, 215, 230, 220}, {239, 212, 226, 209, 233}, false},
    {"frente 105 ruido", FRENTE, {92, 118, 105, 119, 95}, {110, 91, 116, 99, 104}, true},
    {"frente 165 ruido", FRENTE, {152, 178, 160, 170, 155}, {175, 151, 168, 158, 163}, false},
    /* Frente com picos isolados */
    {"frente aberta, pico baixo", FRENTE, {224, 224, 60, 224, 224}, {224, 224, 224, 224, 224},
     false},
    {"frente parede, pico alto", FRENTE, {44, 44, 1200, 44, 44}, {44, 44, 44, 44, 44}, true},
    {"frente aberta, 8190 no fim", FRENTE, {224, 224, 224, 224, 8190},
     {224, 224, 224, 224, 224}, false},
    {"frente parede, 8190 no fim", FRENTE, {44, 44, 44, 44, 8190}, {44, 44, 44, 44, 44}, true},
    /* Frente: histerese */
    {"frente parede, faixa mantém", FRENTE, {100, 100, 100, 135, 135, 135},
     {100, 100, 100, 135, 135, 135}, true},
    {"frente aberta, faixa mantém", FRENTE, {200, 200, 200, 135, 135, 135},
     {200, 200, 200, 135, 135, 135}, false},
    {"frente parede some", FRENTE, {100, 100, 100, 160, 160, 160},
     {100, 100, 100, 160, 160, 160}, false},
    {"frente parede aparece", FRENTE, {200, 200, 200, 115, 115, 115},
     {200, 200, 200, 115, 115, 115}, true},
    {"frente oscila no limiar", FRENTE, {118, 122, 118, 122, 118, 122},
     {118, 122, 118, 122, 118, 122}, true},
    {"frente parede, oscila na faixa", FRENTE, {100, 100, 100, 125, 145, 130, 140, 135},
     {100, 100, 100, 125, 145, 130, 140, 135}, true},
    {"frente aberta, oscila na faixa", FRENTE, {200, 200, 200, 125, 145, 130, 140, 135},
     {200, 200, 200, 125, 145, 130, 140, 135}, false},
    /* Frente: um frontal só */
    {"frente só o esquerdo vê", FRENTE, {44, 44, 44, 44}, {SEM, SEM, SEM, SEM}, true},
    {"frente só o direito, aberta", FRENTE, {SEM, SEM, SEM, SEM}, {224, 224, 224, 224}, false},
    {"frente só o esquerdo, aberta", FRENTE, {224, 224, 224, 224}, {SEM, SEM, SEM, SEM}, false},
    {"frente sem nenhum", FRENTE, {SEM, SEM, SEM, SEM}, {SEM, SEM, SEM, SEM}, false},
    {"frente média dilui um errado", FRENTE, {44, 44, 44}, {300, 300, 300}, false},
    {"frente média 100 e 130", FRENTE, {100, 100, 100}, {130, 130, 130}, true},
    {"frente pico no direito", FRENTE, {44, 44, 44}, {44, 44, 1200}, true},
    /* Frente: falhas */
    {"frente 2 falhas toleradas", FRENTE, {44, SEM, SEM, 44, 44}, {44, 44, 44, 44, 44}, true},
    {"frente esquerdo cai, direito aberto", FRENTE, {SEM, SEM, SEM, 44, 44},
     {224, 224, 224, 224, 224}, false},
    /* Lateral presente */
    {"esquerda 77", ESQUERDA, {77, 77, 77, 77, 77}, {NADA}, true},
    {"esquerda 54", ESQUERDA, {54, 54, 54}, {NADA}, true},
    {"esquerda 139", ESQUERDA, {139, 139, 139}, {NADA}, true},
    {"direita 77", DIREITA, {77, 77, 77, 77, 77}, {NADA}, true},
    /* Lateral ausente */
    {"esquerda 331", ESQUERDA, {331, 331, 331}, {NADA}, false},
    {"direita 232", DIREITA, {232, 232, 232}, {NADA}, false},
    {"esquerda 8190", ESQUERDA, {8190, 8190, 8190}, {NADA}, false},
    {"direita 171", DIREITA, {171, 171, 171}, {NADA}, false},
    /* Lateral com ruído */
    {"esquerda 77 ruido", ESQUERDA, {62, 92, 70, 85, 77}, {NADA}, true},
    {"direita 331 ruido", DIREITA, {316, 346, 320, 340, 331}, {NADA}, false},
    {"esquerda 160 ruido, sem parede antes", ESQUERDA, {150, 170, 160, 155, 165}, {NADA},
     false},
    /* Lateral com picos */
    {"direita aberta, pico baixo", DIREITA, {331, 331, 77, 331, 331}, {NADA}, false},
    {"esquerda parede, pico alto", ESQUERDA, {77, 77, 1200, 77, 77}, {NADA}, true},
    /* Lateral: histerese */
    {"esquerda parede, faixa mantém", ESQUERDA, {100, 100, 100, 160, 160, 160}, {NADA}, true},
    {"esquerda parede some", ESQUERDA, {100, 100, 100, 180, 180, 180}, {NADA}, false},
    {"esquerda aberta, faixa mantém", ESQUERDA, {300, 300, 300, 150, 150, 150}, {NADA}, false},
    /* Lateral: falhas */
    {"esquerda cai e não volta", ESQUERDA, {SEM, SEM, SEM, 77, 77}, {NADA}, false},
    {"direita 2 falhas toleradas", DIREITA, {77, SEM, SEM, 77}, {NADA}, true},
};

#define QTD_CASOS ((int)(sizeof(CASOS) / sizeof(CASOS[0])))

static int tamanho_seq(const uint16_t *seq)
{
    int n = 0;
    while (n < MAX_SEQ && seq[n] != NADA) {
        n++;
    }
    return n;
}

static void inserir(tof_filtro_t *f, uint16_t leitura)
{
    tof_filtro_inserir(f, leitura != SEM, leitura == SEM ? 0 : leitura);
}

static bool rodar_caso(const caso_t *c)
{
    tof_filtro_t filtros[TOF_QTD];
    paredes_t estado;

    for (int i = 0; i < TOF_QTD; i++) {
        tof_filtro_iniciar(&filtros[i]);
    }
    paredes_iniciar(&estado);

    int n = tamanho_seq(c->a);
    for (int i = 0; i < n; i++) {
        switch (c->lado) {
        case FRENTE:
            inserir(&filtros[TOF_FRONTAL_ESQ], c->a[i]);
            inserir(&filtros[TOF_FRONTAL_DIR], c->b[i]);
            break;
        case ESQUERDA:
            inserir(&filtros[TOF_ESQUERDO], c->a[i]);
            break;
        case DIREITA:
            inserir(&filtros[TOF_DIREITO], c->a[i]);
            break;
        }
        paredes_atualizar(&estado, filtros);
    }

    switch (c->lado) {
    case FRENTE:
        return estado.frente;
    case ESQUERDA:
        return estado.esquerda;
    default:
        return estado.direita;
    }
}

static void test_50_casos_sinteticos(void)
{
    char mensagem[96];

    TEST_ASSERT_EQUAL_INT(50, QTD_CASOS);
    for (int i = 0; i < QTD_CASOS; i++) {
        snprintf(mensagem, sizeof(mensagem), "caso %d: %s", i + 1, CASOS[i].descricao);
        TEST_ASSERT_EQUAL_MESSAGE(CASOS[i].esperado, rodar_caso(&CASOS[i]), mensagem);
    }
}

/* ------------------------------------------------- ponta a ponta (HAL sim) */

/* Robô em (0, 0), 10 mm depois de entrar na célula, virado para o norte. */
static void pose_na_janela_lateral(void)
{
    sim_definir_pose(CELULA_MM / 2.0f, 10.0f, PI_F / 2.0f);
}

static void ler_ciclos(int ciclos)
{
    for (int i = 0; i < ciclos; i++) {
        sim_mundo_avancar(50);
        sensores_ler();
    }
}

static void test_4_sensores_leem_a_20_hz(void)
{
    uint32_t inicio = sim_tempo_ms();
    int leituras[TOF_QTD] = {0};

    while (sim_tempo_ms() - inicio < 1000) {
        sim_mundo_avancar(50);
        sensores_ler();
        for (int i = 0; i < TOF_QTD; i++) {
            uint16_t mm;
            if (sensores_distancia_mm((tof_id_t)i, &mm)) {
                leituras[i]++;
            }
        }
    }
    for (int i = 0; i < TOF_QTD; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL_INT(20, leituras[i]);
    }
}

static void test_largada_ve_paredes_laterais_e_frente_aberta(void)
{
    pose_na_janela_lateral();
    ler_ciclos(3);

    paredes_t p = sensores_paredes();
    TEST_ASSERT_FALSE(p.frente);
    TEST_ASSERT_TRUE(p.esquerda);
    TEST_ASSERT_TRUE(p.direita);
}

static void test_celula_aberta_dos_lados_com_ruido(void)
{
    sim_definir_pose(X_CELULA_ABERTA, 10.0f, PI_F / 2.0f);
    sim_definir_tof_ruido(15, 1234);

    for (int i = 0; i < 40; i++) {
        ler_ciclos(1);
        if (i < 2) {
            continue; /* a janela da mediana ainda está enchendo */
        }
        paredes_t p = sensores_paredes();
        TEST_ASSERT_FALSE(p.frente);
        TEST_ASSERT_FALSE(p.esquerda);
        TEST_ASSERT_FALSE(p.direita);
    }
}

static void test_paredes_estaveis_com_ruido(void)
{
    pose_na_janela_lateral();
    sim_definir_tof_ruido(15, 42);

    for (int i = 0; i < 40; i++) {
        ler_ciclos(1);
        paredes_t p = sensores_paredes();
        TEST_ASSERT_FALSE(p.frente);
        TEST_ASSERT_TRUE(p.esquerda);
        TEST_ASSERT_TRUE(p.direita);
    }
}

static void test_pico_isolado_nao_cria_parede(void)
{
    sim_definir_pose(X_CELULA_ABERTA, 10.0f, PI_F / 2.0f);
    ler_ciclos(3);
    TEST_ASSERT_FALSE(sensores_paredes().direita);

    sim_definir_tof_pico(TOF_DIREITO, 40);
    ler_ciclos(1);
    TEST_ASSERT_FALSE(sensores_paredes().direita);
    ler_ciclos(3);
    TEST_ASSERT_FALSE(sensores_paredes().direita);
}

static void test_frente_ve_parede_ao_chegar_no_fim(void)
{
    /* (0, 1): centro da célula do topo, parede norte a ~44 mm do frontal. */
    sim_definir_pose(CELULA_MM / 2.0f, CELULA_MM * 1.5f, PI_F / 2.0f);
    ler_ciclos(3);
    TEST_ASSERT_TRUE(sensores_paredes().frente);
}

/*
 * Janela da leitura lateral (config/robo.h), no pior caso: coluna do meio sem
 * paredes laterais e vizinhas com parede horizontal em toda fronteira.
 */
static const char CORREDOR_ABERTO[] =
    "+---+---+---+\n"
    "|           |\n"
    "+---+   +---+\n"
    "|           |\n"
    "+---+   +---+\n"
    "|           |\n"
    "+---+---+---+\n";

static const char CORREDOR_FECHADO[] =
    "+---+---+---+\n"
    "|   |   |   |\n"
    "+---+   +---+\n"
    "|   |   |   |\n"
    "+---+   +---+\n"
    "|   |   |   |\n"
    "+---+---+---+\n";

/* Anda pela coluna do meio, de 60 mm antes até 20 mm depois de entrar em (1, 1). */
static paredes_t atravessar_fronteira(const char *labirinto)
{
    sim_mundo_iniciar(labirinto);
    sensores_iniciar();
    for (int p = -60; p <= 20; p += 20) { /* 20 mm por amostra: 400 mm/s a 20 Hz */
        sim_definir_pose(CELULA_MM * 1.5f, CELULA_MM + p, PI_F / 2.0f);
        sensores_ler();
    }
    return sensores_paredes();
}

static void test_lateral_lida_ao_entrar_na_celula(void)
{
    paredes_t aberto = atravessar_fronteira(CORREDOR_ABERTO);
    TEST_ASSERT_FALSE(aberto.esquerda);
    TEST_ASSERT_FALSE(aberto.direita);

    paredes_t fechado = atravessar_fronteira(CORREDOR_FECHADO);
    TEST_ASSERT_TRUE(fechado.esquerda);
    TEST_ASSERT_TRUE(fechado.direita);
}

/* Lida tarde demais (p = 60 mm), o lateral vê a parede da vizinha. */
static void test_lateral_tarde_demais_ve_a_vizinha(void)
{
    sim_mundo_iniciar(CORREDOR_ABERTO);
    sim_definir_pose(CELULA_MM * 1.5f, CELULA_MM + 60.0f, PI_F / 2.0f);
    ler_ciclos(3);
    TEST_ASSERT_TRUE(sensores_paredes().direita);
}

/* No centro de uma célula sem paredes laterais, o lateral vê o poste do canto. */
static void test_lateral_no_centro_ve_o_poste(void)
{
    sim_definir_pose(X_CELULA_ABERTA, CELULA_MM / 2.0f, PI_F / 2.0f);
    ler_ciclos(3);
    TEST_ASSERT_TRUE(sensores_paredes().direita);
}

static void test_sensor_ausente_reportado_com_o_nome(void)
{
    pose_na_janela_lateral();
    ler_ciclos(3);
    TEST_ASSERT_FALSE(sensores_tof_falhou(TOF_DIREITO));

    sim_definir_tof_ausente(TOF_DIREITO, true);
    ler_ciclos(2);
    TEST_ASSERT_FALSE(sensores_tof_falhou(TOF_DIREITO));
    ler_ciclos(1);
    TEST_ASSERT_TRUE(sensores_tof_falhou(TOF_DIREITO));
    TEST_ASSERT_FALSE(sensores_paredes().direita); /* falho conta como sem parede */
    uint16_t mm;
    TEST_ASSERT_FALSE(sensores_distancia_mm(TOF_DIREITO, &mm));

    /* Voltar a responder não basta: o sensor fica falho até sensores_iniciar. */
    sim_definir_tof_ausente(TOF_DIREITO, false);
    ler_ciclos(3);
    TEST_ASSERT_TRUE(sensores_tof_falhou(TOF_DIREITO));
    sensores_iniciar();
    ler_ciclos(1);
    TEST_ASSERT_FALSE(sensores_tof_falhou(TOF_DIREITO));
    TEST_ASSERT_TRUE(sensores_paredes().direita);
    sim_definir_tof_ausente(TOF_DIREITO, true);
    TEST_ASSERT_EQUAL_STRING("tof_direito", hal_tof_nome(TOF_DIREITO));

    tof_autoteste_t r[TOF_QTD];
    tof_autoteste_executar(r);
    TEST_ASSERT_EQUAL_STRING("tof_direito", r[TOF_DIREITO].nome);
    TEST_ASSERT_FALSE(r[TOF_DIREITO].aprovado);
    TEST_ASSERT_FALSE(r[TOF_DIREITO].tem_valor); /* valor vai como null */
    TEST_ASSERT_TRUE(r[TOF_ESQUERDO].aprovado);
}

/* --------------------------------------------------------------- autoteste */

static void test_autoteste_na_largada(void)
{
    pose_na_janela_lateral();

    tof_autoteste_t r[TOF_QTD];
    tof_autoteste_executar(r);
    for (int i = 0; i < TOF_QTD; i++) {
        TEST_ASSERT_TRUE_MESSAGE(r[i].aprovado, r[i].nome);
        TEST_ASSERT_TRUE(r[i].tem_valor);
        TEST_ASSERT_EQUAL_STRING(hal_tof_nome((tof_id_t)i), r[i].nome);
    }
    TEST_ASSERT_UINT16_WITHIN(5, 77, r[TOF_ESQUERDO].valor_mm);
    TEST_ASSERT_UINT16_WITHIN(5, 77, r[TOF_DIREITO].valor_mm);
}

static void test_autoteste_regras(void)
{
    const bool todas[5] = {true, true, true, true, true};
    const bool uma_falha[5] = {true, true, false, true, true};
    const uint16_t perto[5] = {80, 79, 81, 300, 78};
    const uint16_t tampado[5] = {5, 6, 4, 5, 5};
    const uint16_t nada[5] = {8190, 8190, 8190, 8190, 8190};
    const uint16_t limite[5] = {20, 20, 20, 20, 20};
    const bool nenhuma[5] = {false, false, false, false, false};
    bool tem_valor;
    uint16_t valor;

    TEST_ASSERT_TRUE(tof_autoteste_avaliar(todas, perto, 5, &tem_valor, &valor));
    TEST_ASSERT_TRUE(tem_valor);
    TEST_ASSERT_EQUAL_UINT16(80, valor);

    /* Uma falha reprova, mas o valor das que responderam é informado. */
    TEST_ASSERT_FALSE(tof_autoteste_avaliar(uma_falha, perto, 5, &tem_valor, &valor));
    TEST_ASSERT_TRUE(tem_valor);
    TEST_ASSERT_EQUAL_UINT16(79, valor); /* 78, 79, 80, 300: média dos dois do meio */

    TEST_ASSERT_FALSE(tof_autoteste_avaliar(todas, tampado, 5, &tem_valor, &valor));
    TEST_ASSERT_EQUAL_UINT16(5, valor);
    TEST_ASSERT_TRUE(tof_autoteste_avaliar(todas, limite, 5, &tem_valor, &valor));

    TEST_ASSERT_FALSE(tof_autoteste_avaliar(nenhuma, perto, 5, &tem_valor, &valor));
    TEST_ASSERT_FALSE(tem_valor);

    /* Nada à vista é aprovado: corredor aberto na frente do sensor. */
    TEST_ASSERT_TRUE(tof_autoteste_avaliar(todas, nada, 5, &tem_valor, &valor));
    TEST_ASSERT_EQUAL_UINT16(TOF_ALCANCE_MAX_MM, valor);

    /* Bordas de n: zero reprova; acima do máximo, o excesso é ignorado. */
    TEST_ASSERT_FALSE(tof_autoteste_avaliar(todas, perto, 0, &tem_valor, &valor));
    TEST_ASSERT_FALSE(tem_valor);
    TEST_ASSERT_TRUE(tof_autoteste_avaliar(todas, perto, 99, &tem_valor, &valor));
}

/* ------------------------------------------------------------------ XSHUT */

static void test_sequencia_xshut(void)
{
    tof_passo_t passos[TOF_PASSOS_INICIO];
    const int8_t pinos[TOF_QTD] = {PINO_XSHUT_FE, PINO_XSHUT_FD, PINO_XSHUT_E, PINO_XSHUT_D};

    TEST_ASSERT_EQUAL_INT(TOF_PASSOS_INICIO, tof_sequencia_inicio(passos));
    TEST_ASSERT_EQUAL_INT(17, TOF_PASSOS_INICIO);

    for (int i = 0; i < TOF_QTD; i++) {
        TEST_ASSERT_EQUAL_INT(TOF_PASSO_XSHUT_BAIXO, passos[i].tipo);
        TEST_ASSERT_EQUAL_INT8(pinos[i], passos[i].pino);
    }
    TEST_ASSERT_EQUAL_INT(TOF_PASSO_ESPERAR_MS, passos[4].tipo);

    for (int i = 0; i < TOF_QTD; i++) {
        const tof_passo_t *p = &passos[5 + 3 * i];
        TEST_ASSERT_EQUAL_INT(TOF_PASSO_XSHUT_ALTO, p[0].tipo);
        TEST_ASSERT_EQUAL_INT8(pinos[i], p[0].pino);
        TEST_ASSERT_EQUAL_INT(TOF_PASSO_ESPERAR_MS, p[1].tipo);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(2, p[1].valor);
        TEST_ASSERT_EQUAL_INT(TOF_PASSO_TROCAR_ENDERECO, p[2].tipo);
        TEST_ASSERT_EQUAL_INT8(i, p[2].sensor);
        TEST_ASSERT_EQUAL_HEX8(0x30 + i, p[2].valor);
    }
}

/* Em nenhum momento dois sensores ligados respondem no endereço padrão. */
static void test_xshut_nunca_dois_no_endereco_padrao(void)
{
    tof_passo_t passos[TOF_PASSOS_INICIO];
    bool ligado[TOF_QTD] = {true, true, true, true}; /* XSHUT solto no boot */
    uint8_t endereco[TOF_QTD] = {0x29, 0x29, 0x29, 0x29};
    int n = tof_sequencia_inicio(passos);
    bool trocou_algum = false;

    for (int k = 0; k < n; k++) {
        const tof_passo_t *p = &passos[k];
        if (p->tipo == TOF_PASSO_XSHUT_BAIXO) {
            ligado[p->sensor] = false;
            endereco[p->sensor] = TOF_ENDERECO_PADRAO; /* desligar volta ao padrão */
        } else if (p->tipo == TOF_PASSO_XSHUT_ALTO) {
            ligado[p->sensor] = true;
        } else if (p->tipo == TOF_PASSO_TROCAR_ENDERECO) {
            endereco[p->sensor] = p->valor;
            trocou_algum = true;
        }

        if (trocou_algum || p->tipo == TOF_PASSO_XSHUT_ALTO) {
            int no_padrao = 0;
            for (int i = 0; i < TOF_QTD; i++) {
                no_padrao += ligado[i] && endereco[i] == TOF_ENDERECO_PADRAO;
            }
            TEST_ASSERT_LESS_OR_EQUAL_INT(1, no_padrao);
        }
    }

    for (int i = 0; i < TOF_QTD; i++) {
        TEST_ASSERT_TRUE(ligado[i]);
        TEST_ASSERT_EQUAL_HEX8(tof_endereco((tof_id_t)i), endereco[i]);
        for (int j = i + 1; j < TOF_QTD; j++) {
            TEST_ASSERT_NOT_EQUAL(endereco[i], endereco[j]);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mediana_de_3_5_e_par);
    RUN_TEST(test_nada_a_vista_satura_no_alcance);
    RUN_TEST(test_filtro_tolera_duas_falhas_e_descarta_na_terceira);
    RUN_TEST(test_janela_da_mediana_desliza);
    RUN_TEST(test_histerese);
    RUN_TEST(test_50_casos_sinteticos);
    RUN_TEST(test_4_sensores_leem_a_20_hz);
    RUN_TEST(test_largada_ve_paredes_laterais_e_frente_aberta);
    RUN_TEST(test_celula_aberta_dos_lados_com_ruido);
    RUN_TEST(test_paredes_estaveis_com_ruido);
    RUN_TEST(test_pico_isolado_nao_cria_parede);
    RUN_TEST(test_frente_ve_parede_ao_chegar_no_fim);
    RUN_TEST(test_lateral_no_centro_ve_o_poste);
    RUN_TEST(test_lateral_lida_ao_entrar_na_celula);
    RUN_TEST(test_lateral_tarde_demais_ve_a_vizinha);
    RUN_TEST(test_sensor_ausente_reportado_com_o_nome);
    RUN_TEST(test_autoteste_na_largada);
    RUN_TEST(test_autoteste_regras);
    RUN_TEST(test_sequencia_xshut);
    RUN_TEST(test_xshut_nunca_dois_no_endereco_padrao);
    return UNITY_END();
}
