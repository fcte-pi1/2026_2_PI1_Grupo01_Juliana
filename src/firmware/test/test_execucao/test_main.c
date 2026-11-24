/*
 * Execução das ações da navegação com as primitivas de movimento, em malha fechada no mundo
 * simulado: o robô anda e gira de verdade (motores, encoders e ToF do mundo simulado).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "config/robo.h"
#include "corrida.h"
#include "execucao.h"
#include "hal_motor.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "movimento.h"
#include "sensores.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"
#include "../../src/app/labirinto_demo.h"

#define CICLOS_POR_LEITURA 10 /* 50 ms: leitura dos ToF a 20 Hz */
#define ESPERA_PARADO_MS   200
#define TEMPO_MAX_MS       180000

/* 3x3 sem paredes internas, para testar cada ação a partir da célula do meio. */
static const char LABIRINTO_3X3[] =
    "+---+---+---+\n"
    "|           |\n"
    "+   +   +   +\n"
    "|           |\n"
    "+   +   +   +\n"
    "|           |\n"
    "+---+---+---+\n";

static corrida_t c;
static corrida_efeitos_t ef;
static uint32_t agora_ms;

/* Na ESP32 a NVS é a da flash e abre uma vez só; no PC é a simulada em RAM. */
static void iniciar_nvs(void)
{
#ifdef ARDUINO
    static bool iniciada;
    if (!iniciada) {
        iniciada = hal_nvs_iniciar();
    }
#else
    (void)hal_nvs_iniciar();
#endif
}

static void limpar_nvs(void)
{
#ifdef ARDUINO
    iniciar_nvs();
    hal_nvs_apagar(MEM_CHAVE_MAPA);
    hal_nvs_apagar(MEM_CHAVE_TIPOS);
#else
    hal_nvs_sim_apagar_tudo();
#endif
}

void setUp(void)
{
    limpar_nvs();
    agora_ms = 0;
}

void tearDown(void) {}

static void informar_tof(void)
{
    paredes_t p = sensores_paredes();
    uint16_t f = 0, e = 0, d = 0;
    sensores_distancia_mm(TOF_FRONTAL, &f);
    sensores_distancia_mm(TOF_ESQUERDO, &e);
    sensores_distancia_mm(TOF_DIREITO, &d);
    mov_tof_t t = {p.frente, p.esquerda, p.direita, f, e, d};
    mov_informar_tof(&t);
}

/* Um ciclo de 5 ms: o mundo anda, os ToF são lidos a cada 50 ms e a execução avança. */
static exec_estado_t ciclo(void)
{
    sim_mundo_avancar(MOV_PERIODO_MS);
    agora_ms += MOV_PERIODO_MS;
    if ((agora_ms / MOV_PERIODO_MS) % CICLOS_POR_LEITURA == 0) {
        sensores_ler();
        informar_tof();
    }
    return exec_atualizar();
}

static void iniciar_mundo(const char *labirinto)
{
    sim_mundo_iniciar(labirinto);
    (void)hal_tof_iniciar();
    iniciar_nvs();
    mov_iniciar();
    sensores_iniciar();
    exec_iniciar();
}

static void ate_ocioso(void)
{
    uint32_t limite = agora_ms + 10000;
    while (exec_estado() != EXEC_OCIOSO && exec_estado() != EXEC_FALHOU && agora_ms < limite) {
        ciclo();
    }
}

/* Coloca o robô no centro da célula, virado para a direção, e deixa os ToF estabilizarem. */
static void colocar(int x, int y, nav_direcao_t direcao)
{
    sim_mundo_no_centro_da_celula(x, y, (int)direcao);
    for (int i = 0; i < 20; i++) {
        ciclo();
    }
}

/* O chassi (não o eixo, que fica atrás do centro) termina no centro da célula, sem ter encostado. */
static void conferir_no_centro(int x, int y)
{
    sim_pose_t p = sim_centro_do_chassi();
    float centro_x = (float)x * CELULA_MM + CELULA_MM / 2.0f;
    float centro_y = (float)y * CELULA_MM + CELULA_MM / 2.0f;
    TEST_ASSERT_FALSE(p.colidiu);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(20.0f, centro_x, p.x_mm, "x fora do centro da celula");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(20.0f, centro_y, p.y_mm, "y fora do centro da celula");
}

static void test_frente_avanca_uma_celula_para_o_norte(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 0, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    ate_ocioso();
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
    conferir_no_centro(1, 1);
}

static void test_esquerda_gira_e_avanca_para_o_oeste(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_ESQUERDA));
    /* Sem parede à frente, o eixo vai primeiro ao centro da célula: o giro não desloca o robô de lado. */
    TEST_ASSERT_EQUAL_INT(EXEC_AVANCANDO, exec_estado());
    ate_ocioso();
    conferir_no_centro(0, 1);
}

static void test_direita_gira_e_avanca_para_o_leste(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_DIREITA));
    ate_ocioso();
    conferir_no_centro(2, 1);
}

static void test_meia_volta_gira_e_avanca_para_o_sul(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_MEIA_VOLTA));
    ate_ocioso();
    conferir_no_centro(1, 0);
}

/*
 * Labirinto com becos e cantos, para as manobras do robô montado: a quina da frente gira a ~100 mm do eixo,
 * e do meio da célula à parede há 82,5 mm.
 *   (0, 0) a (0, 2): corredor que termina num beco em (0, 2).
 *   (1, 0): canto, com parede à frente (leste) e saída para o norte.
 */
static const char LABIRINTO_MANOBRAS[] =
    "+---+---+\n"
    "|   |   |\n"
    "+   +   +\n"
    "|   |   |\n"
    "+   +   +\n"
    "|       |\n"
    "+---+---+\n";

/* Uma ação inteira no mundo simulado, sem encostar em nada em nenhum momento. */
static void executar_sem_encostar(nav_acao_t acao)
{
    TEST_ASSERT_TRUE(exec_comecar(acao));
    uint32_t limite = agora_ms + 15000;
    while (exec_estado() != EXEC_OCIOSO && exec_estado() != EXEC_FALHOU && agora_ms < limite) {
        ciclo();
        TEST_ASSERT_FALSE_MESSAGE(sim_pose().colidiu, "encostou na parede");
    }
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
}

/* Meia-volta no beco: o giro de 180° no lugar bateria; a manobra cabe e o robô sai centrado. */
static void test_meia_volta_no_beco_nao_encosta(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    colocar(0, 2, NAV_NORTE);
    executar_sem_encostar(NAV_ACAO_MEIA_VOLTA);
    conferir_no_centro(0, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -PI_F / 2.0f, sim_pose().theta_rad < -PI_F ? sim_pose().theta_rad + 2 * PI_F
                                                                                 : sim_pose().theta_rad);
}

/* Meia-volta com o robô já fora do meio (a lateral que a execução tem guardada), para os dois lados. */
static void test_meia_volta_com_o_robo_deslocado(void)
{
    for (int lado = -1; lado <= 1; lado += 2) {
        iniciar_mundo(LABIRINTO_MANOBRAS);
        colocar(0, 2, NAV_NORTE);
        sim_pose_t p = sim_pose();
        sim_definir_pose(p.x_mm - 8.0f * (float)lado, p.y_mm, p.theta_rad); /* norte: esquerda é -x */
        mov_definir_lateral_mm(8.0f * (float)lado);
        executar_sem_encostar(NAV_ACAO_MEIA_VOLTA);
        conferir_no_centro(0, 1);
    }
}

/* Giro no canto, com parede à frente: o robô avança só o que a quina deixa, gira e sai centrado. */
static void test_giro_no_canto_com_parede_a_frente_nao_encosta(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    colocar(1, 0, NAV_LESTE);
    executar_sem_encostar(NAV_ACAO_ESQUERDA);
    conferir_no_centro(1, 1);
}

/* Corredor reto com uma roda andando 3% a mais: os laterais seguram o robô no meio. */
static void test_corredor_com_deriva_fica_no_meio(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    colocar(0, 0, NAV_NORTE);
    sim_definir_escala_roda(MOTOR_DIR, 1.03f);
    executar_sem_encostar(NAV_ACAO_FRENTE);
    executar_sem_encostar(NAV_ACAO_FRENTE);
    sim_definir_escala_roda(MOTOR_DIR, 1.0f);
    conferir_no_centro(0, 2);
}

/*
 * Pista, 24/11/2026: ao chegar na primeira célula com parede à frente, o robô passou do ponto, encostou nela e
 * empurrou até o tempo limite (falha stuck). Aqui o frontal lê a parede 30 mm mais longe do que ela está:
 * o avanço não pode esticar por isso, e a meia-volta seguinte cabe.
 */
static void test_frontal_lendo_longe_nao_faz_passar_do_ponto(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    sim_definir_tof_desvio(TOF_FRONTAL, 30);
    colocar(0, 0, NAV_NORTE);
    executar_sem_encostar(NAV_ACAO_FRENTE);
    executar_sem_encostar(NAV_ACAO_FRENTE);
    conferir_no_centro(0, 2);
    executar_sem_encostar(NAV_ACAO_MEIA_VOLTA);
    sim_definir_tof_desvio(TOF_FRONTAL, 0);
}

/*
 * Se mesmo assim encostar (aqui as rodas andam 20% a mais que os pulsos e o frontal não responde), o avanço
 * conta como chegada com o nariz na parede, em vez de falhar; e a ação seguinte parte dessa posição.
 */
static void test_encostar_na_parede_no_fim_conta_como_chegada(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    colocar(0, 1, NAV_NORTE);
    sim_definir_tof_ausente(TOF_FRONTAL, true);
    sim_definir_escala_roda(MOTOR_ESQ, 1.2f);
    sim_definir_escala_roda(MOTOR_DIR, 1.2f);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    ate_ocioso();
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
    TEST_ASSERT_TRUE(mov_parou_na_parede());
    TEST_ASSERT_TRUE(sim_pose().colidiu); /* encostou de verdade */

    sim_definir_tof_ausente(TOF_FRONTAL, false);
    sim_definir_escala_roda(MOTOR_ESQ, 1.0f);
    sim_definir_escala_roda(MOTOR_DIR, 1.0f);
    for (int i = 0; i < 20; i++) {
        ciclo(); /* os ToF voltam a ter leitura */
    }
    executar_sem_encostar(NAV_ACAO_MEIA_VOLTA);
    conferir_no_centro(0, 1);
}

/* Encostado na parede de um canto, a curva recua mais antes do arco e não encosta. */
static void test_curva_depois_de_encostar_recua_mais(void)
{
    iniciar_mundo(LABIRINTO_MANOBRAS);
    colocar(0, 0, NAV_LESTE); /* (0, 0) para o leste: (1, 0) é um canto, com parede à frente e saída ao norte */
    sim_definir_tof_ausente(TOF_FRONTAL, true);
    sim_definir_escala_roda(MOTOR_ESQ, 1.2f);
    sim_definir_escala_roda(MOTOR_DIR, 1.2f);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    ate_ocioso();
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
    TEST_ASSERT_TRUE(mov_parou_na_parede()); /* encostou na parede leste de (1, 0) */
    sim_definir_tof_ausente(TOF_FRONTAL, false);
    sim_definir_escala_roda(MOTOR_ESQ, 1.0f);
    sim_definir_escala_roda(MOTOR_DIR, 1.0f);
    for (int i = 0; i < 20; i++) {
        ciclo();
    }
    executar_sem_encostar(NAV_ACAO_ESQUERDA);
    conferir_no_centro(1, 1);
}

static void test_acoes_que_nao_movem_sao_recusadas(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    TEST_ASSERT_FALSE(exec_comecar(NAV_ACAO_SUCESSO));
    TEST_ASSERT_FALSE(exec_comecar(NAV_ACAO_SEM_PASSO));
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
}

static void test_nao_comeca_outra_acao_com_uma_em_andamento(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    TEST_ASSERT_FALSE(exec_comecar(NAV_ACAO_ESQUERDA));
}

static void test_roda_travada_faz_a_execucao_falhar_e_parar(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    sim_travar_encoder(MOTOR_ESQ, true);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    ate_ocioso();
    TEST_ASSERT_EQUAL_INT(EXEC_FALHOU, exec_estado());
    TEST_ASSERT_EQUAL_INT(MOV_FALHOU, mov_estado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

static void test_parar_cancela_a_execucao_e_desliga_os_motores(void)
{
    iniciar_mundo(LABIRINTO_3X3);
    colocar(1, 1, NAV_NORTE);
    TEST_ASSERT_TRUE(exec_comecar(NAV_ACAO_FRENTE));
    for (int i = 0; i < 20; i++) {
        ciclo();
    }
    exec_parar();
    TEST_ASSERT_EQUAL_INT(EXEC_OCIOSO, exec_estado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

/* Resultado de uma corrida andando no mundo simulado. */
typedef struct {
    bool sucesso;
    bool encostou;
    int passos;
} corrida_andando_t;

/*
 * A corrida inteira, andando de verdade: decide, executa, para, decide. O labirinto já está carregado, com a
 * largada em (0, 0) virada para o norte (o referencial do robô).
 */
static corrida_andando_t correr_andando(nav_tipo_t tipo, uint32_t tempo_max_ms)
{
    corrida_andando_t r = {false, false, 0};
    corrida_iniciar(&c, tipo);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef); /* o robô liga ocioso e espera o comando */
    sim_mundo_no_centro_da_celula(c.nav.x, c.nav.y, (int)c.nav.direcao);
    corrida_health_check_concluido(&c, true, tipo, &ef);
    corrida_evento(&c, MAQ_EV_CMD_LARGAR, &ef);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));

    for (int i = 0; i < 20; i++) {
        ciclo();
    }
    uint32_t limite = agora_ms + tempo_max_ms;
    uint32_t parado_desde = agora_ms;
    while (agora_ms < limite && maq_andando(&c.maq)) {
        exec_estado_t e = ciclo();
        r.encostou = r.encostou || sim_pose().colidiu;
        if (e == EXEC_FALHOU) {
            corrida_movimento_falhou(&c, &ef);
            break;
        }
        if (e != EXEC_OCIOSO) {
            parado_desde = agora_ms;
            continue;
        }
        if (agora_ms - parado_desde < ESPERA_PARADO_MS) {
            continue;
        }
        paredes_t p = sensores_paredes();
        nav_leitura_t leitura = {p.frente, p.esquerda, p.direita}; /* paredes lidas parado */
        bool reposicionar = false;
        telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX];
        int n = corrida_decidir(&c, leitura, &reposicionar, saida);
        for (int k = 0; k < n; k++) {
            r.sucesso = r.sucesso || saida[k].tipo == TELEMETRIA_DECISAO_SUCESSO;
        }
        r.passos++;
        if (exec_eh_movimento(c.acao)) {
            TEST_ASSERT_TRUE(exec_comecar(c.acao));
        }
        parado_desde = agora_ms;
    }
    return r;
}

/* A corrida inteira no labirinto de demonstração, andando de verdade. */
static void test_corrida_completa_andando_chega_ao_objetivo(void)
{
    iniciar_mundo(LABIRINTO_DEMO);
    corrida_andando_t r = correr_andando(ROBO_TIPO_LABIRINTO, TEMPO_MAX_MS);
    TEST_ASSERT_TRUE_MESSAGE(r.sucesso, "a corrida nao chegou ao objetivo");
    TEST_ASSERT_FALSE_MESSAGE(r.encostou, "encostou na parede");
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_SUCCESS, corrida_estado_contrato(&c));
    TEST_ASSERT_TRUE(r.passos < 80);
    conferir_no_centro(3, 3);
}

/*
 * Os labirintos do FIRM-02 (simulador-navegacao/labirintos, versões -dir), andando de verdade e com o robô
 * do jeito que ele é: perto da parede o ToF lê de 30 a 40 mm, cada leitura tem até ±3 mm de ruído e a roda
 * direita anda 0,5% a mais do que os pulsos dizem (os encoders medidos diferem 0,35%). Nenhuma corrida pode
 * encostar numa parede. Com ±5 mm de ruído ou 1% a mais, algumas corridas encostam (docs/decisoes/024).
 */
#define PASTA_LABIRINTOS "simulador-navegacao/labirintos/"

static const char *const LABIRINTOS_DIR[] = {
    "4x4-01",          "4x4-02",          "4x4-03",         "4x4-04",
    "8x4-lateral-01",  "8x4-lateral-02",  "8x4-frente-01",  "8x4-frente-02",
    "12x4-lateral-01", "12x4-lateral-02", "12x4-frente-01", "12x4-frente-02",
};

static bool carregar_labirinto(const char *nome, char *texto, size_t tamanho)
{
    char caminho[128];
    snprintf(caminho, sizeof(caminho), PASTA_LABIRINTOS "%s-dir.txt", nome);
    FILE *f = fopen(caminho, "r");
    if (f == NULL) {
        return false;
    }
    size_t n = fread(texto, 1, tamanho - 1, f);
    fclose(f);
    texto[n] = '\0';
    return true;
}

static void test_corridas_andando_nos_labirintos_do_projeto(void)
{
    static char texto[4096];
    char mensagem[96];
    int corridas = 0;
    for (size_t i = 0; i < sizeof(LABIRINTOS_DIR) / sizeof(LABIRINTOS_DIR[0]); i++) {
        TEST_ASSERT_TRUE_MESSAGE(carregar_labirinto(LABIRINTOS_DIR[i], texto, sizeof(texto)), LABIRINTOS_DIR[i]);
        iniciar_mundo(texto);
        const sim_labirinto_t *lab = sim_labirinto();
        if (lab->largada_x != 0 || lab->largada_y != 0 || sim_labirinto_tem_parede(lab, 0, 0, SIM_PAREDE_N)) {
            continue; /* o referencial do robô pede a largada em (0, 0), saindo para o norte */
        }
        limpar_nvs();
        sim_definir_tof_curto(true);
        sim_definir_tof_ruido(3, 7u + (uint32_t)i);
        sim_definir_escala_roda(MOTOR_DIR, 1.005f);
        nav_tipo_t tipo = lab->largura == 12 || lab->altura == 12 ? NAV_TIPO_12X4
                          : lab->largura == 8 || lab->altura == 8 ? NAV_TIPO_8X4
                                                                  : NAV_TIPO_4X4;
        corrida_andando_t r = correr_andando(tipo, 900000);
        snprintf(mensagem, sizeof(mensagem), "%s: sucesso %d, encostou %d, %d passos", LABIRINTOS_DIR[i],
                 r.sucesso, r.encostou, r.passos);
        TEST_ASSERT_TRUE_MESSAGE(r.sucesso && !r.encostou, mensagem);
        corridas++;
    }
    TEST_ASSERT_TRUE(corridas >= 4);
}

static void test_movimento_falhou_vira_falha_automatica_stuck_e_grava_failed(void)
{
    iniciar_mundo(LABIRINTO_DEMO);
    corrida_iniciar(&c, ROBO_TIPO_LABIRINTO);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef); /* o robô liga ocioso e espera o comando */
    corrida_health_check_concluido(&c, true, ROBO_TIPO_LABIRINTO, &ef);
    corrida_evento(&c, MAQ_EV_CMD_LARGAR, &ef);

    corrida_movimento_falhou(&c, &ef);
    TEST_ASSERT_TRUE(ef.parar_motores);
    TEST_ASSERT_TRUE(ef.tem_falha);
    TEST_ASSERT_EQUAL_INT(CONTRATO_MOTIVO_STUCK, ef.falha.motivo);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ORIGEM_AUTOMATICA, ef.falha.origem);
    TEST_ASSERT_EQUAL_INT(CONTRATO_COMPONENTE_NENHUM, ef.falha.componente);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)c.nav.x, ef.falha.x);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)c.nav.y, ef.falha.y);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, corrida_estado_contrato(&c));

    corrida_iniciar(&c, ROBO_TIPO_LABIRINTO);
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA, c.inicio);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, c.estado);
}

static void test_movimento_falhou_fora_da_corrida_nao_faz_nada(void)
{
    iniciar_mundo(LABIRINTO_DEMO);
    corrida_iniciar(&c, ROBO_TIPO_LABIRINTO);
    corrida_movimento_falhou(&c, &ef);
    TEST_ASSERT_FALSE(ef.tem_falha);
    TEST_ASSERT_FALSE(ef.parar_motores);
}

#ifdef ARDUINO
/* Na ESP32 o ponto de entrada é o setup() de entrada_arduino.cpp. */
int executar_testes(void)
#else
int main(void)
#endif
{
    UNITY_BEGIN();
    RUN_TEST(test_frente_avanca_uma_celula_para_o_norte);
    RUN_TEST(test_esquerda_gira_e_avanca_para_o_oeste);
    RUN_TEST(test_direita_gira_e_avanca_para_o_leste);
    RUN_TEST(test_meia_volta_gira_e_avanca_para_o_sul);
    RUN_TEST(test_meia_volta_no_beco_nao_encosta);
    RUN_TEST(test_meia_volta_com_o_robo_deslocado);
    RUN_TEST(test_giro_no_canto_com_parede_a_frente_nao_encosta);
    RUN_TEST(test_corredor_com_deriva_fica_no_meio);
    RUN_TEST(test_frontal_lendo_longe_nao_faz_passar_do_ponto);
    RUN_TEST(test_encostar_na_parede_no_fim_conta_como_chegada);
    RUN_TEST(test_curva_depois_de_encostar_recua_mais);
    RUN_TEST(test_acoes_que_nao_movem_sao_recusadas);
    RUN_TEST(test_nao_comeca_outra_acao_com_uma_em_andamento);
    RUN_TEST(test_roda_travada_faz_a_execucao_falhar_e_parar);
    RUN_TEST(test_parar_cancela_a_execucao_e_desliga_os_motores);
    RUN_TEST(test_corrida_completa_andando_chega_ao_objetivo);
    RUN_TEST(test_corridas_andando_nos_labirintos_do_projeto);
    RUN_TEST(test_movimento_falhou_vira_falha_automatica_stuck_e_grava_failed);
    RUN_TEST(test_movimento_falhou_fora_da_corrida_nao_faz_nada);
    return UNITY_END();
}
