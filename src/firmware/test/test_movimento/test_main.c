/*
 * Testes das primitivas de movimento (FIRM-05, #135) com os encoders do mundo
 * simulado: os pulsos contados batem com os calculados para 180 mm e 90°.
 *
 * Pulsos esperados, com a roda de 34 mm, 640 pulsos por volta e bitola de 92,5 mm:
 *   avanço de 180 mm:  180 / (pi x 34 / 640)                ~ 1078,6 -> 1079
 *   giro de 90°:       (pi x 92,5 / 4) / (pi x 34 / 640)    ~  435,3 ->  435
 *   giro de 180°:      o dobro do arco                      ~  870,6 ->  871
 */

#include <math.h>
#include <unity.h>

#include "config/robo.h"
#include "hal_encoder.h"
#include "movimento.h"
#include "sim_mundo.h"

#define PULSOS_180_MM  1079
#define PULSOS_90_GRAUS 435
#define PULSOS_180_GRAUS 871

/* 2x2 sem paredes internas: o robô anda e gira sem encostar em nada. */
static const char LABIRINTO_2X2[] =
    "+---+---+\n"
    "|       |\n"
    "+   +   +\n"
    "|       |\n"
    "+---+---+\n";

static const char LABIRINTO_1X1[] =
    "+---+\n"
    "|   |\n"
    "+---+\n";

void setUp(void)
{
    sim_mundo_iniciar(LABIRINTO_2X2);
    mov_iniciar();
}

void tearDown(void) {}

/* Roda o laço de controle até o movimento acabar; devolve o estado final. */
static mov_estado_t rodar(uint32_t *duracao_ms)
{
    mov_estado_t estado = MOV_EM_CURSO;
    uint32_t ms = 0;
    while (estado == MOV_EM_CURSO && ms < 2 * MOV_TEMPO_LIMITE_MS) {
        sim_mundo_avancar(MOV_PERIODO_MS);
        estado = mov_atualizar();
        ms += MOV_PERIODO_MS;
    }
    if (duracao_ms) {
        *duracao_ms = ms;
    }
    return estado;
}

static float angulo_normalizado(float a)
{
    while (a > PI_F) {
        a -= 2.0f * PI_F;
    }
    while (a <= -PI_F) {
        a += 2.0f * PI_F;
    }
    return a;
}

static void test_pulsos_calculados(void)
{
    TEST_ASSERT_EQUAL_INT32(PULSOS_180_MM, mov_pulsos_avanco(CELULA_MM));
    TEST_ASSERT_EQUAL_INT32(PULSOS_90_GRAUS, mov_pulsos_giro(90));
    TEST_ASSERT_EQUAL_INT32(-PULSOS_90_GRAUS, mov_pulsos_giro(-90));
    TEST_ASSERT_EQUAL_INT32(PULSOS_180_GRAUS, mov_pulsos_giro(180));
}

static void test_avancar_celula_conta_os_pulsos_de_180_mm(void)
{
    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));

    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_DIR));

    /* Saiu do centro de A1 e parou no centro da célula de cima, sem desviar. */
    sim_pose_t p = sim_pose();
    TEST_ASSERT_FLOAT_WITHIN(1.5f, CELULA_MM / 2.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(1.5f, CELULA_MM * 1.5f, p.y_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, PI_F / 2.0f, p.theta_rad);
    TEST_ASSERT_FALSE(p.colidiu);
}

static void test_girar_90_conta_os_pulsos_de_90_graus(void)
{
    TEST_ASSERT_TRUE(mov_girar(90));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));

    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, -PULSOS_90_GRAUS, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, PULSOS_90_GRAUS, hal_encoder_ler(MOTOR_DIR));

    /* Começa virado para o norte; girar para a esquerda deixa virado para o oeste, no mesmo lugar. */
    sim_pose_t p = sim_pose();
    TEST_ASSERT_FLOAT_WITHIN(0.02f, PI_F, fabsf(angulo_normalizado(p.theta_rad)));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, CELULA_MM / 2.0f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, CELULA_MM / 2.0f, p.y_mm);
}

static void test_andado_acompanha_o_avanco_e_fica_em_zero_no_giro(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, mov_andado_mm());

    TEST_ASSERT_TRUE(mov_avancar_celula());
    for (int i = 0; i < 40; i++) {
        sim_mundo_avancar(MOV_PERIODO_MS);
        mov_atualizar();
    }
    float no_meio = mov_andado_mm();
    TEST_ASSERT_TRUE(no_meio > 0.0f && no_meio < CELULA_MM);
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_FLOAT_WITHIN(2.0f, (float)CELULA_MM, mov_andado_mm());

    TEST_ASSERT_TRUE(mov_girar(90));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, mov_andado_mm());
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, mov_andado_mm());
}

/* Um motor real não anda com potência pequena: o fim do movimento precisa vencer essa zona morta. */
static void test_avanco_e_giro_concluem_com_zona_morta_no_motor(void)
{
    sim_definir_zona_morta(80);
    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_DIR));

    TEST_ASSERT_TRUE(mov_girar(90));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    sim_definir_zona_morta(0);
}

static void test_girar_menos_90_vai_para_a_direita(void)
{
    TEST_ASSERT_TRUE(mov_girar(-90));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));

    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, PULSOS_90_GRAUS, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, -PULSOS_90_GRAUS, hal_encoder_ler(MOTOR_DIR));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, angulo_normalizado(sim_pose().theta_rad));
}

static void test_girar_180_volta_para_tras(void)
{
    TEST_ASSERT_TRUE(mov_girar(180));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));

    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, -PULSOS_180_GRAUS, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_PULSOS, PULSOS_180_GRAUS, hal_encoder_ler(MOTOR_DIR));
    TEST_ASSERT_FLOAT_WITHIN(0.02f, -PI_F / 2.0f, angulo_normalizado(sim_pose().theta_rad));
}

static void test_angulo_invalido_nao_move(void)
{
    TEST_ASSERT_FALSE(mov_girar(45));
    TEST_ASSERT_FALSE(mov_girar(0));
    TEST_ASSERT_FALSE(mov_girar(270));
    TEST_ASSERT_EQUAL(MOV_LIVRE, mov_atualizar());
    sim_mundo_avancar(200);
    TEST_ASSERT_EQUAL_INT32(0, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(0, hal_encoder_ler(MOTOR_DIR));
}

static void test_nao_aceita_outro_movimento_em_curso(void)
{
    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_FALSE(mov_avancar_celula());
    TEST_ASSERT_FALSE(mov_girar(90));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_TRUE(mov_girar(90));
}

/* Velocidade média das rodas, em mm/s, a cada janela de 20 ms. */
static void test_rampa_acelera_e_freia_sem_degrau(void)
{
    enum { JANELA_MS = 20 };
    float v[200];
    int n = 0;
    int32_t anterior = 0;
    mov_estado_t estado = MOV_EM_CURSO;

    TEST_ASSERT_TRUE(mov_avancar_celula());
    while (estado == MOV_EM_CURSO && n < 200) {
        for (int i = 0; i < JANELA_MS / MOV_PERIODO_MS && estado == MOV_EM_CURSO; i++) {
            sim_mundo_avancar(MOV_PERIODO_MS);
            estado = mov_atualizar();
        }
        int32_t atual = (hal_encoder_ler(MOTOR_ESQ) + hal_encoder_ler(MOTOR_DIR)) / 2;
        v[n++] = (float)(atual - anterior) * MM_POR_PULSO * 1000.0f / JANELA_MS;
        anterior = atual;
    }
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, estado);

    float maxima = 0.0f;
    int pico = 0;
    for (int i = 0; i < n; i++) {
        if (v[i] > maxima) {
            maxima = v[i];
            pico = i;
        }
    }
    /* Chega perto do cruzeiro sem passar dele. */
    TEST_ASSERT_TRUE(maxima > VELOCIDADE_CRUZEIRO_MM_S * 0.9f);
    TEST_ASSERT_TRUE(maxima < VELOCIDADE_CRUZEIRO_MM_S * 1.05f);
    /* Sai e chega devagar: nem a primeira nem a última janela têm degrau de velocidade. */
    TEST_ASSERT_TRUE(v[0] < 100.0f);
    TEST_ASSERT_TRUE(v[n - 1] < 100.0f);
    /* Até o pico a velocidade só cresce, e não mais que a aceleração permite (com folga da contagem). */
    for (int i = 1; i <= pico; i++) {
        float aceleracao = (v[i] - v[i - 1]) * 1000.0f / JANELA_MS;
        TEST_ASSERT_TRUE(aceleracao > -500.0f);
        TEST_ASSERT_TRUE(aceleracao < MOV_ACELERACAO_MM_S2 * 1.5f);
    }
    /* Não passa do ponto: no fim, no máximo a ré de 2 pulsos numa janela. */
    for (int i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(v[i] > -30.0f);
    }
}

static void test_sequencia_de_primitivas_chega_na_celula_certa(void)
{
    /* A1 -> A2 (norte), vira à direita, A2 -> B2 (leste). */
    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_TRUE(mov_girar(-90));
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));
    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, rodar(NULL));

    /* Cada avanço pode terminar a MOV_TOLERANCIA_AVANCO_PULSOS (1,3 mm) do alvo: dois avanços, até 3 mm. */
    sim_pose_t p = sim_pose();
    TEST_ASSERT_FLOAT_WITHIN(3.0f, CELULA_MM * 1.5f, p.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, CELULA_MM * 1.5f, p.y_mm);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.0f, angulo_normalizado(p.theta_rad));
    TEST_ASSERT_FALSE(p.colidiu);
}

/* As correções pelos ToF ficam desligadas até o FIRM-10: uma leitura torta não muda o avanço. */
static void test_correcoes_pelos_tof_desligadas(void)
{
    const mov_tof_t torto = {false, true, false, 0, 40, 0};

    TEST_ASSERT_TRUE(mov_avancar_celula());
    mov_estado_t estado = MOV_EM_CURSO;
    while (estado == MOV_EM_CURSO) {
        mov_informar_tof(&torto);
        sim_mundo_avancar(MOV_PERIODO_MS);
        estado = mov_atualizar();
    }
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, estado);
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_INT32_WITHIN(MOV_TOLERANCIA_AVANCO_PULSOS, PULSOS_180_MM, hal_encoder_ler(MOTOR_DIR));
}

/* Parede a 70 mm logo na partida: o robô anda um pouco e para, a tempo de sobrar ao menos 1 cm até ela. */
static void test_obstaculo_a_frente_para_o_avanco(void)
{
    const mov_tof_t perto = {true, false, false, 70, 0, 0};

    TEST_ASSERT_TRUE(mov_avancar_celula());
    mov_estado_t estado = MOV_EM_CURSO;
    while (estado == MOV_EM_CURSO) {
        mov_informar_tof(&perto);
        sim_mundo_avancar(MOV_PERIODO_MS);
        estado = mov_atualizar();
    }
    TEST_ASSERT_EQUAL(MOV_FALHOU, estado);
    TEST_ASSERT_TRUE(mov_andado_mm() < 70.0f - MOV_FRENTE_SOBRA_MM);
}

/* Parede onde o avanço previsto termina: a leitura chega atrasada, como a do filtro, e não dispara a parada. */
static void test_parede_no_fim_da_celula_nao_para_o_avanco(void)
{
    TEST_ASSERT_TRUE(mov_avancar_celula());
    mov_estado_t estado = MOV_EM_CURSO;
    float anterior = 0.0f;
    while (estado == MOV_EM_CURSO) {
        float andado = mov_andado_mm();
        float velocidade = (andado - anterior) / (MOV_PERIODO_MS / 1000.0f);
        anterior = andado;
        float falta = (float)CELULA_MM - andado;
        mov_tof_t t = {true, false, false, (uint16_t)(MOV_FRENTE_ALVO_MM + falta + velocidade * MOV_TOF_ATRASO_S), 0, 0};
        mov_informar_tof(&t);
        sim_mundo_avancar(MOV_PERIODO_MS);
        estado = mov_atualizar();
    }
    TEST_ASSERT_EQUAL(MOV_CONCLUIDO, estado);
}

static void test_parar_corta_o_movimento(void)
{
    TEST_ASSERT_TRUE(mov_avancar_celula());
    for (int i = 0; i < 30; i++) {
        sim_mundo_avancar(MOV_PERIODO_MS);
        mov_atualizar();
    }
    mov_parar();
    TEST_ASSERT_EQUAL(MOV_LIVRE, mov_estado());

    /* Depois do atraso do motor, as rodas param bem antes da célula seguinte. */
    sim_mundo_avancar(600);
    int32_t parado = hal_encoder_ler(MOTOR_ESQ);
    sim_mundo_avancar(300);
    TEST_ASSERT_EQUAL_INT32(parado, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_TRUE(parado < PULSOS_180_MM / 2);
    TEST_ASSERT_EQUAL(MOV_LIVRE, mov_atualizar());
}

static void test_parede_na_frente_faz_o_avanco_falhar(void)
{
    sim_mundo_iniciar(LABIRINTO_1X1);
    mov_iniciar();
    uint32_t duracao = 0;

    TEST_ASSERT_TRUE(mov_avancar_celula());
    TEST_ASSERT_EQUAL(MOV_FALHOU, rodar(&duracao));
    /* A roda travada para o movimento bem antes do tempo limite e desliga a ponte H. */
    TEST_ASSERT_TRUE(duracao < MOV_TEMPO_LIMITE_MS / 2);
    TEST_ASSERT_FALSE(sim_motores_habilitados());
    TEST_ASSERT_TRUE(sim_pose().colidiu);
    TEST_ASSERT_TRUE(hal_encoder_ler(MOTOR_ESQ) < PULSOS_180_MM);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pulsos_calculados);
    RUN_TEST(test_avancar_celula_conta_os_pulsos_de_180_mm);
    RUN_TEST(test_girar_90_conta_os_pulsos_de_90_graus);
    RUN_TEST(test_andado_acompanha_o_avanco_e_fica_em_zero_no_giro);
    RUN_TEST(test_avanco_e_giro_concluem_com_zona_morta_no_motor);
    RUN_TEST(test_girar_menos_90_vai_para_a_direita);
    RUN_TEST(test_girar_180_volta_para_tras);
    RUN_TEST(test_angulo_invalido_nao_move);
    RUN_TEST(test_nao_aceita_outro_movimento_em_curso);
    RUN_TEST(test_rampa_acelera_e_freia_sem_degrau);
    RUN_TEST(test_sequencia_de_primitivas_chega_na_celula_certa);
    RUN_TEST(test_correcoes_pelos_tof_desligadas);
    RUN_TEST(test_obstaculo_a_frente_para_o_avanco);
    RUN_TEST(test_parede_no_fim_da_celula_nao_para_o_avanco);
    RUN_TEST(test_parar_corta_o_movimento);
    RUN_TEST(test_parede_na_frente_faz_o_avanco_falhar);
    return UNITY_END();
}
