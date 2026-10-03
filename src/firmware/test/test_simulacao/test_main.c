#include <math.h>
#include <unity.h>

#include "config/robo.h"
#include "sim_labirinto.h"
#include "sim_mundo.h"

/* Corredor reto de 1x3: largada embaixo, parede no fim. */
static const char CORREDOR_1X3[] =
    "+---+\n"
    "|   |\n"
    "+   +\n"
    "|   |\n"
    "+   +\n"
    "|   |\n"
    "+---+\n";

/* Corredor de 1x8 (1,44 m): a parede do fim fica fora do alcance do ToF. */
static const char CORREDOR_1X8[] =
    "+---+\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n"
    "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n" "+   +\n" "|   |\n"
    "+---+\n";

void setUp(void) {}
void tearDown(void) {}

static void test_labirinto_le_paredes_dos_dois_lados(void)
{
    sim_labirinto_t lab;
    TEST_ASSERT_TRUE(sim_labirinto_carregar(&lab,
        "# comentário ignorado\n"
        "+---+---+\n"
        "|   |   |\n"
        "+   +---+\n"
        "|       |\n"
        "+---+---+\n"));

    TEST_ASSERT_EQUAL_UINT8(2, lab.largura);
    TEST_ASSERT_EQUAL_UINT8(2, lab.altura);

    /* Parede vertical entre (0,1) e (1,1) aparece nas duas células. */
    TEST_ASSERT_TRUE(sim_labirinto_tem_parede(&lab, 0, 1, SIM_PAREDE_L));
    TEST_ASSERT_TRUE(sim_labirinto_tem_parede(&lab, 1, 1, SIM_PAREDE_O));
    /* Parede horizontal entre (1,0) e (1,1). */
    TEST_ASSERT_TRUE(sim_labirinto_tem_parede(&lab, 1, 0, SIM_PAREDE_N));
    TEST_ASSERT_TRUE(sim_labirinto_tem_parede(&lab, 1, 1, SIM_PAREDE_S));
    /* Passagens. */
    TEST_ASSERT_FALSE(sim_labirinto_tem_parede(&lab, 0, 0, SIM_PAREDE_N));
    TEST_ASSERT_FALSE(sim_labirinto_tem_parede(&lab, 0, 0, SIM_PAREDE_L));
    /* Fora do labirinto é sempre parede. */
    TEST_ASSERT_TRUE(sim_labirinto_tem_parede(&lab, 5, 5, SIM_PAREDE_N));
}

static void test_labirinto_rejeita_desenho_malformado(void)
{
    sim_labirinto_t lab;
    TEST_ASSERT_FALSE(sim_labirinto_carregar(&lab, ""));
    TEST_ASSERT_FALSE(sim_labirinto_carregar(&lab, "|   |\n+---+\n|   |\n"));
    TEST_ASSERT_FALSE(sim_labirinto_carregar(&lab, "+---+\n|   |\n"));
}

static void test_tof_mede_distancia_ate_as_paredes(void)
{
    TEST_ASSERT_TRUE(sim_mundo_iniciar(CORREDOR_1X3));
    uint16_t mm;

    /* Frente: sensor em y = 90 + 40, parede do fim em y = 540 - 6. */
    TEST_ASSERT_TRUE(sim_tof_medir(0, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 404, mm);

    /* Laterais a 45°: sensor a 30 mm do centro, paredes a 84 mm do centro,
     * então o feixe percorre 54 / sen(45°) = 76 mm. */
    TEST_ASSERT_TRUE(sim_tof_medir(2, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 76, mm);
    TEST_ASSERT_TRUE(sim_tof_medir(3, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 76, mm);
}

static void test_tof_fora_de_alcance(void)
{
    TEST_ASSERT_TRUE(sim_mundo_iniciar(CORREDOR_1X8));
    uint16_t mm;
    TEST_ASSERT_TRUE(sim_tof_medir(0, &mm));
    TEST_ASSERT_EQUAL_UINT16(SIM_TOF_FORA_DE_ALCANCE, mm);
}

static void test_motores_desabilitados_nao_movem(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_motor_definir(0, 800);
    sim_motor_definir(1, 800);
    sim_mundo_avancar(1000);

    TEST_ASSERT_EQUAL_INT32(0, sim_encoder(0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, sim_pose().y_mm);
}

static void test_encoders_acompanham_o_deslocamento(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_motor_habilitar(true);
    sim_motor_definir(0, 300);
    sim_motor_definir(1, 300);
    sim_mundo_avancar(1000);

    sim_pose_t pose = sim_pose();
    float andou_mm = pose.y_mm - 90.0f;
    float mm_por_pulso = 3.14159265f * RODA_DIAMETRO_MM / ENCODER_PULSOS_POR_VOLTA;

    TEST_ASSERT_FALSE(pose.colidiu);
    TEST_ASSERT_TRUE(andou_mm > 200.0f);           /* ~250 mm/s menos a aceleração */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, pose.x_mm); /* andou reto */
    TEST_ASSERT_INT32_WITHIN(2, (int32_t)(andou_mm / mm_por_pulso), sim_encoder(0));
    TEST_ASSERT_EQUAL_INT32(sim_encoder(0), sim_encoder(1));
}

static void test_colisao_trava_robo_e_encoders(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_motor_habilitar(true);
    sim_motor_definir(0, 1000);
    sim_motor_definir(1, 1000);
    sim_mundo_avancar(3000);

    sim_pose_t pose = sim_pose();
    TEST_ASSERT_TRUE(pose.colidiu);
    TEST_ASSERT_TRUE(pose.y_mm <= 540.0f - PAREDE_ESPESSURA_MM / 2.0f - ROBO_RAIO_MM);

    int32_t pulsos = sim_encoder(0);
    sim_mundo_avancar(500);
    TEST_ASSERT_EQUAL_INT32(pulsos, sim_encoder(0));
}

static void test_giro_no_lugar(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_motor_habilitar(true);
    sim_motor_definir(0, -300);
    sim_motor_definir(1, 300);
    sim_mundo_avancar(500);

    sim_pose_t pose = sim_pose();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, pose.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, pose.y_mm);
    TEST_ASSERT_TRUE(pose.theta_rad > 3.14159265f / 2.0f); /* girou para a esquerda */
    TEST_ASSERT_EQUAL_INT32(-sim_encoder(1), sim_encoder(0));
}

static void test_bateria_descarrega_e_passa_pelo_divisor(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    TEST_ASSERT_UINT16_WITHIN(1, (uint16_t)(BATERIA_CHEIA_MV / BATERIA_DIVISOR), sim_bateria_adc_mv());

    sim_mundo_avancar(60000);
    TEST_ASSERT_TRUE(sim_bateria_adc_mv() < (uint16_t)(BATERIA_CHEIA_MV / BATERIA_DIVISOR));

    sim_definir_bateria_mv(7000);
    TEST_ASSERT_UINT16_WITHIN(1, (uint16_t)(7000 / BATERIA_DIVISOR), sim_bateria_adc_mv());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_labirinto_le_paredes_dos_dois_lados);
    RUN_TEST(test_labirinto_rejeita_desenho_malformado);
    RUN_TEST(test_tof_mede_distancia_ate_as_paredes);
    RUN_TEST(test_tof_fora_de_alcance);
    RUN_TEST(test_motores_desabilitados_nao_movem);
    RUN_TEST(test_encoders_acompanham_o_deslocamento);
    RUN_TEST(test_colisao_trava_robo_e_encoders);
    RUN_TEST(test_giro_no_lugar);
    RUN_TEST(test_bateria_descarrega_e_passa_pelo_divisor);
    return UNITY_END();
}
