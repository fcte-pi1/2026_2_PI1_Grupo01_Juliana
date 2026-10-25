/*
 * Velocidade por roda — FIRM-04 (#130). Critérios de aceite:
 *   - motores parados no boot e no reset;
 *   - a velocidade fica a até 5 % da referência no motor simulado;
 *   - encoder parado gera stuck dentro de MOTOR_TRAVADO_MS.
 */

#include <unity.h>

#include "config/robo.h"
#include "hal_encoder.h"
#include "hal_tempo.h"
#include "sim_mundo.h"
#include "velocidade.h"

#define REF_MM_S 400.0f

/* Corredor de 1x8: a 400 mm/s o robô não alcança a parede do fim no teste. */
static const char CORREDOR_1X8[] =
    "+---+\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n"
    "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n"
    "+---+\n";

void setUp(void)
{
    sim_mundo_iniciar(CORREDOR_1X8);
    vel_iniciar();
}

void tearDown(void) {}

static void avancar_malha(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        sim_mundo_avancar(5);
        vel_passo();
    }
}

static float mm_s_entre(int32_t antes, int32_t depois, uint32_t ms)
{
    float mm_por_pulso = PI_F * RODA_DIAMETRO_MM / (float)ENCODER_PULSOS_POR_VOLTA;
    return (float)(depois - antes) * mm_por_pulso / (ms * 0.001f);
}

static float modulo(float v) { return v < 0.0f ? -v : v; }

/* A roda simulada ainda anda um pouco depois do corte. 1 s deixa ela parar. */
static void esperar_costa(void) { sim_mundo_avancar(1000); }

static void conferir_velocidade(float ref_esq, float ref_dir)
{
    int32_t esq_antes;
    int32_t dir_antes;

    vel_definir_mm_s(MOTOR_ESQ, ref_esq);
    vel_definir_mm_s(MOTOR_DIR, ref_dir);
    avancar_malha(500);

    esq_antes = hal_encoder_ler(MOTOR_ESQ);
    dir_antes = hal_encoder_ler(MOTOR_DIR);
    avancar_malha(200);

    TEST_ASSERT_FLOAT_WITHIN(modulo(ref_esq) * 0.05f, ref_esq,
                             mm_s_entre(esq_antes, hal_encoder_ler(MOTOR_ESQ), 200));
    TEST_ASSERT_FLOAT_WITHIN(modulo(ref_dir) * 0.05f, ref_dir,
                             mm_s_entre(dir_antes, hal_encoder_ler(MOTOR_DIR), 200));
    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_pose().colidiu);
}

/* Encoders já travados e referências já pedidas. O stuck sai em MOTOR_TRAVADO_MS. */
static void conferir_trava_no_prazo(void)
{
    uint32_t t_liga = 0;
    uint32_t t_trava = 0;
    bool ligado = false;

    for (int i = 0; i < MOTOR_TRAVADO_MS / 5 + 3; i++) {
        sim_mundo_avancar(5);
        vel_passo();
        if (!ligado && sim_motores_habilitados()) {
            ligado = true;
            t_liga = hal_tempo_ms();
        }
        if (ligado && hal_tempo_ms() - t_liga < MOTOR_TRAVADO_MS) {
            TEST_ASSERT_FALSE(vel_travado());
            TEST_ASSERT_TRUE(sim_motores_habilitados());
        }
        if (vel_travado() && t_trava == 0) {
            t_trava = hal_tempo_ms();
        }
    }

    TEST_ASSERT_TRUE(ligado);
    TEST_ASSERT_TRUE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
    TEST_ASSERT_EQUAL_UINT32(MOTOR_TRAVADO_MS, t_trava - t_liga);

    sim_mundo_avancar(20);
    vel_passo();
    TEST_ASSERT_TRUE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

static void test_boot_deixa_os_motores_parados(void)
{
    int32_t esq;
    int32_t dir;

    /* Como se a ponte tivesse ficado ligada antes do boot. */
    sim_motor_habilitar(true);
    sim_motor_definir(MOTOR_ESQ, MOTOR_POTENCIA_MAX);
    sim_motor_definir(MOTOR_DIR, MOTOR_POTENCIA_MAX);

    vel_iniciar();
    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());

    esq = hal_encoder_ler(MOTOR_ESQ);
    dir = hal_encoder_ler(MOTOR_DIR);
    sim_mundo_avancar(500);
    TEST_ASSERT_EQUAL_INT32(esq, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(dir, hal_encoder_ler(MOTOR_DIR));

    avancar_malha(100);
    TEST_ASSERT_FALSE(sim_motores_habilitados());
    TEST_ASSERT_EQUAL_INT32(esq, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(dir, hal_encoder_ler(MOTOR_DIR));
}

static void test_reset_para_os_motores(void)
{
    int32_t esq;
    int32_t dir;

    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);
    avancar_malha(200);
    TEST_ASSERT_TRUE(hal_encoder_ler(MOTOR_ESQ) > 0);
    TEST_ASSERT_TRUE(sim_motores_habilitados());

    vel_iniciar();
    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());

    esperar_costa();
    esq = hal_encoder_ler(MOTOR_ESQ);
    dir = hal_encoder_ler(MOTOR_DIR);
    avancar_malha(200);
    TEST_ASSERT_EQUAL_INT32(esq, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(dir, hal_encoder_ler(MOTOR_DIR));
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

static void test_converge_para_a_referencia(void) { conferir_velocidade(REF_MM_S, REF_MM_S); }

static void test_converge_para_outra_referencia(void) { conferir_velocidade(200.0f, 200.0f); }

static void test_converge_na_re(void)
{
    /* No meio do corredor: a ré não bate na parede de trás durante a medida. */
    sim_definir_pose(CELULA_MM / 2.0f, 4.0f * CELULA_MM, PI_F / 2.0f);
    conferir_velocidade(-300.0f, -300.0f);
}

static void test_passo_so_age_a_cada_5_ms(void)
{
    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);

    for (int i = 0; i < 4; i++) {
        sim_mundo_avancar(1);
        vel_passo();
        TEST_ASSERT_FALSE(sim_motores_habilitados());
    }

    sim_mundo_avancar(1);
    vel_passo();
    TEST_ASSERT_TRUE(sim_motores_habilitados());
}

static void test_encoder_parado_trava_no_prazo(void)
{
    sim_travar_encoder(MOTOR_ESQ, true);
    sim_travar_encoder(MOTOR_DIR, true);
    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);
    conferir_trava_no_prazo();
}

static void test_uma_roda_parada_trava_no_prazo(void)
{
    sim_travar_encoder(MOTOR_ESQ, true);
    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);
    conferir_trava_no_prazo();

    TEST_ASSERT_EQUAL_INT32(0, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_TRUE(hal_encoder_ler(MOTOR_DIR) > 0);
}

static void test_reset_depois_do_stuck_continua_parado(void)
{
    sim_travar_encoder(MOTOR_ESQ, true);
    sim_travar_encoder(MOTOR_DIR, true);
    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);
    conferir_trava_no_prazo();

    vel_iniciar();
    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());

    avancar_malha(MOTOR_TRAVADO_MS);
    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

static void test_encoder_andando_nao_trava(void)
{
    vel_definir_mm_s(MOTOR_ESQ, REF_MM_S);
    vel_definir_mm_s(MOTOR_DIR, REF_MM_S);
    avancar_malha(MOTOR_TRAVADO_MS + 500);

    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_TRUE(sim_motores_habilitados());
    TEST_ASSERT_FALSE(sim_pose().colidiu);
}

static void test_referencia_zero_nao_trava(void)
{
    sim_travar_encoder(MOTOR_ESQ, true);
    sim_travar_encoder(MOTOR_DIR, true);
    vel_definir_mm_s(MOTOR_ESQ, 0.0f);
    vel_definir_mm_s(MOTOR_DIR, 0.0f);
    avancar_malha(MOTOR_TRAVADO_MS + 50);

    TEST_ASSERT_FALSE(vel_travado());
    TEST_ASSERT_FALSE(sim_motores_habilitados());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_deixa_os_motores_parados);
    RUN_TEST(test_reset_para_os_motores);
    RUN_TEST(test_converge_para_a_referencia);
    RUN_TEST(test_converge_para_outra_referencia);
    RUN_TEST(test_converge_na_re);
    RUN_TEST(test_passo_so_age_a_cada_5_ms);
    RUN_TEST(test_encoder_parado_trava_no_prazo);
    RUN_TEST(test_uma_roda_parada_trava_no_prazo);
    RUN_TEST(test_reset_depois_do_stuck_continua_parado);
    RUN_TEST(test_encoder_andando_nao_trava);
    RUN_TEST(test_referencia_zero_nao_trava);
    return UNITY_END();
}
