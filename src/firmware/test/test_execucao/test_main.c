/*
 * Execução das ações da navegação com as primitivas de movimento, em malha fechada no mundo
 * simulado: o robô anda e gira de verdade (motores, encoders e ToF do mundo simulado).
 */

#include <math.h>
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

static void conferir_no_centro(int x, int y)
{
    sim_pose_t p = sim_pose();
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
    TEST_ASSERT_EQUAL_INT(EXEC_GIRANDO, exec_estado());
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

/* A corrida inteira no labirinto de demonstração, andando de verdade: decide, executa, para, decide. */
static void test_corrida_completa_andando_chega_ao_objetivo(void)
{
    iniciar_mundo(LABIRINTO_DEMO);
    corrida_iniciar(&c, ROBO_TIPO_LABIRINTO);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef); /* o robô liga ocioso e espera o comando */
    sim_mundo_no_centro_da_celula(c.nav.x, c.nav.y, (int)c.nav.direcao);
    corrida_health_check_concluido(&c, true, ROBO_TIPO_LABIRINTO, &ef);
    corrida_evento(&c, MAQ_EV_CMD_LARGAR, &ef);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));

    for (int i = 0; i < 20; i++) {
        ciclo();
    }
    uint32_t parado_desde = agora_ms;
    int passos = 0;
    bool sucesso = false;
    bool tem_lateral = false;
    bool lateral_esq = false;
    bool lateral_dir = false;
    while (agora_ms < TEMPO_MAX_MS && maq_andando(&c.maq)) {
        exec_estado_t e = ciclo();
        /* A parede lateral da célula nova é guardada logo ao entrar nela. */
        if (exec_ler_lateral_agora()) {
            paredes_t q = sensores_paredes();
            lateral_esq = q.esquerda;
            lateral_dir = q.direita;
            tem_lateral = true;
        }
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
        nav_leitura_t leitura = {p.frente, tem_lateral ? lateral_esq : p.esquerda,
                                 tem_lateral ? lateral_dir : p.direita};
        tem_lateral = false;
        bool reposicionar = false;
        telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX];
        int n = corrida_decidir(&c, leitura, &reposicionar, saida);
        for (int k = 0; k < n; k++) {
            sucesso = sucesso || saida[k].tipo == TELEMETRIA_DECISAO_SUCESSO;
        }
        passos++;
        if (exec_eh_movimento(c.acao)) {
            TEST_ASSERT_TRUE(exec_comecar(c.acao));
        }
        parado_desde = agora_ms;
    }
    TEST_ASSERT_TRUE_MESSAGE(sucesso, "a corrida nao chegou ao objetivo");
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_SUCCESS, corrida_estado_contrato(&c));
    TEST_ASSERT_TRUE(passos < 80);
    conferir_no_centro(3, 3);
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
    RUN_TEST(test_acoes_que_nao_movem_sao_recusadas);
    RUN_TEST(test_nao_comeca_outra_acao_com_uma_em_andamento);
    RUN_TEST(test_roda_travada_faz_a_execucao_falhar_e_parar);
    RUN_TEST(test_parar_cancela_a_execucao_e_desliga_os_motores);
    RUN_TEST(test_corrida_completa_andando_chega_ao_objetivo);
    RUN_TEST(test_movimento_falhou_vira_falha_automatica_stuck_e_grava_failed);
    RUN_TEST(test_movimento_falhou_fora_da_corrida_nao_faz_nada);
    return UNITY_END();
}
