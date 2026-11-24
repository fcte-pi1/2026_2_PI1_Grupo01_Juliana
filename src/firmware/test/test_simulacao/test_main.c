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

static void test_robo_no_centro_da_celula_nas_quatro_direcoes(void)
{
    static const char LAB[] = "+---+---+\n|       |\n+---+---+\n";
    TEST_ASSERT_TRUE(sim_mundo_iniciar(LAB));
    const float THETA[4] = {PI_F / 2.0f, 0.0f, -PI_F / 2.0f, PI_F};
    for (int d = 0; d < 4; d++) {
        sim_mundo_no_centro_da_celula(1, 0, d);
        sim_pose_t p = sim_centro_do_chassi();
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f * CELULA_MM, p.x_mm);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f * CELULA_MM, p.y_mm);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, THETA[d], p.theta_rad);
        TEST_ASSERT_FALSE(p.colidiu);
        /* O eixo fica EIXO_RECUO_MM atrás do centro do chassi. */
        sim_pose_t eixo = sim_pose();
        TEST_ASSERT_FLOAT_WITHIN(0.01f, EIXO_RECUO_MM, hypotf(p.x_mm - eixo.x_mm, p.y_mm - eixo.y_mm));
    }
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

    /* Frente: eixo em y = 90 - 31,25, sensor 87,5 mm à frente; parede do fim em y = 540 - 7,5. */
    TEST_ASSERT_TRUE(sim_tof_medir(0, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 386, mm);

    /* Laterais a 90°: sensor a 40 mm do meio, paredes a 82,5 mm do meio: 42,5 mm. */
    TEST_ASSERT_TRUE(sim_tof_medir(1, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 43, mm);
    TEST_ASSERT_TRUE(sim_tof_medir(2, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 43, mm);

    /* Só existem 3 sensores: frontal, esquerdo e direito. */
    TEST_ASSERT_FALSE(sim_tof_medir(3, &mm));
    TEST_ASSERT_FALSE(sim_tof_medir(-1, &mm));
}

static void test_tof_frontal_centrado_ve_a_parede_da_frente(void)
{
    TEST_ASSERT_TRUE(sim_mundo_iniciar(CORREDOR_1X3));
    uint16_t mm;

    /* Chassi centrado na última célula: o nariz fica a ~26 mm da parede. */
    sim_mundo_no_centro_da_celula(0, 2, 0);
    TEST_ASSERT_TRUE(sim_tof_medir(0, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, (uint16_t)MOV_FRENTE_ALVO_MM, mm);

    /* Perto da parede o sensor do robô lê de 30 a 40 mm: o mundo simulado imita isso se pedido. */
    sim_definir_tof_curto(true);
    TEST_ASSERT_TRUE(sim_tof_medir(0, &mm));
    TEST_ASSERT_UINT16_WITHIN(2, 33, mm);
    sim_definir_tof_curto(false);
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
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f - EIXO_RECUO_MM, sim_pose().y_mm);
}

static void test_rodas_no_ar_contam_pulsos_sem_mover_o_robo(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_definir_rodas_no_ar(true);
    sim_motor_habilitar(true);
    sim_motor_definir(0, 800);
    sim_mundo_avancar(1000);

    TEST_ASSERT_GREATER_THAN_INT32(100, sim_encoder(0));
    TEST_ASSERT_EQUAL_INT32(0, sim_encoder(1));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f - EIXO_RECUO_MM, sim_pose().y_mm);
    TEST_ASSERT_FALSE(sim_pose().colidiu);
}

static void test_encoders_acompanham_o_deslocamento(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_motor_habilitar(true);
    sim_motor_definir(0, 300);
    sim_motor_definir(1, 300);
    sim_mundo_avancar(1000);

    sim_pose_t pose = sim_pose();
    float andou_mm = pose.y_mm - (90.0f - EIXO_RECUO_MM);
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
    /* Parou com o nariz na parede do fim. */
    TEST_ASSERT_TRUE(pose.y_mm <= 540.0f - PAREDE_ESPESSURA_MM / 2.0f - EIXO_ATE_FRENTE_MM);
    TEST_ASSERT_TRUE(pose.y_mm > 540.0f - PAREDE_ESPESSURA_MM / 2.0f - EIXO_ATE_FRENTE_MM - 10.0f);

    int32_t pulsos = sim_encoder(0);
    sim_mundo_avancar(500);
    TEST_ASSERT_EQUAL_INT32(pulsos, sim_encoder(0));
}

/* Sem paredes internas: o giro no lugar do robô montado varre ~100 mm em volta do eixo. */
static const char ABERTO_3X3[] =
    "+---+---+---+\n"
    "|           |\n"
    "+   +   +   +\n"
    "|           |\n"
    "+   +   +   +\n"
    "|           |\n"
    "+---+---+---+\n";

static void test_giro_no_lugar(void)
{
    sim_mundo_iniciar(ABERTO_3X3);
    sim_mundo_no_centro_da_celula(1, 1, 0);
    sim_pose_t inicio = sim_pose();
    sim_motor_habilitar(true);
    sim_motor_definir(0, -300);
    sim_motor_definir(1, 300);
    sim_mundo_avancar(200); /* ~60°: um giro maior passaria a quina nos postes de trás */

    sim_pose_t pose = sim_pose();
    TEST_ASSERT_FALSE(pose.colidiu);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, inicio.x_mm, pose.x_mm);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, inicio.y_mm, pose.y_mm);
    TEST_ASSERT_TRUE(pose.theta_rad > 3.14159265f / 2.0f); /* girou para a esquerda */
    TEST_ASSERT_EQUAL_INT32(-sim_encoder(1), sim_encoder(0));
}

/* No corredor (paredes dos dois lados), o giro de 180° no lugar bate: a quina da frente varre ~100 mm. */
static void test_meia_volta_no_lugar_bate_no_corredor(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    sim_mundo_no_centro_da_celula(0, 1, 0);
    sim_motor_habilitar(true);
    sim_motor_definir(0, -300);
    sim_motor_definir(1, 300);
    sim_mundo_avancar(1500);
    TEST_ASSERT_TRUE(sim_pose().colidiu);
}

static void test_bateria_descarrega_e_passa_pelo_divisor(void)
{
    sim_mundo_iniciar(CORREDOR_1X3);
    uint16_t cheia_adc = (uint16_t)((BATERIA_CHEIA_MV - BATERIA_QUEDA_DIODO_MV) / BATERIA_DIVISOR);
    TEST_ASSERT_UINT16_WITHIN(1, cheia_adc, sim_bateria_adc_mv());

    sim_mundo_avancar(60000);
    TEST_ASSERT_TRUE(sim_bateria_adc_mv() < cheia_adc);

    sim_definir_bateria_mv(7000);
    TEST_ASSERT_UINT16_WITHIN(1, (uint16_t)((7000 - BATERIA_QUEDA_DIODO_MV) / BATERIA_DIVISOR),
                              sim_bateria_adc_mv());
}

typedef struct { uint16_t mv; uint8_t pct; } ponto_curva_t;

static void conferir_curva(const ponto_curva_t *c, int n)
{
    for (int i = 1; i < n; i++) {
        TEST_ASSERT_TRUE(c[i].mv < c[i - 1].mv);
        TEST_ASSERT_TRUE(c[i].pct <= c[i - 1].pct);
    }
    TEST_ASSERT_EQUAL_UINT8(100, c[0].pct);
    TEST_ASSERT_EQUAL_UINT8(0, c[n - 1].pct);
}

static void test_curvas_da_bateria_batem_com_os_limites(void)
{
    const ponto_curva_t lipo[] = BATERIA_CURVA_LIPO;
    const ponto_curva_t aa[] = BATERIA_CURVA_AA;
    int n_lipo = (int)(sizeof(lipo) / sizeof(lipo[0]));
    int n_aa = (int)(sizeof(aa) / sizeof(aa[0]));

    conferir_curva(lipo, n_lipo);
    conferir_curva(aa, n_aa);

    TEST_ASSERT_EQUAL_UINT16(BATERIA_CHEIA_MV, lipo[0].mv);
    TEST_ASSERT_EQUAL_UINT16(BATERIA_CORTE_MV, lipo[n_lipo - 1].mv);
    TEST_ASSERT_EQUAL_UINT16(BATERIA_AA_CORTE_MV, aa[n_aa - 1].mv);
    TEST_ASSERT_TRUE(aa[0].mv < BATERIA_LIMIAR_FONTE_MV); /* pilhas cheias não passam por LiPo */
    TEST_ASSERT_TRUE(BATERIA_CORTE_MV > BATERIA_LIMIAR_FONTE_MV);
    TEST_ASSERT_TRUE(BATERIA_ALERTA_MV > BATERIA_CORTE_MV); /* o aviso vem antes do corte */
    TEST_ASSERT_TRUE(BATERIA_ALERTA_MV < BATERIA_CHEIA_MV);
    TEST_ASSERT_TRUE(MOTOR_TENSAO_NOMINAL_MV < BATERIA_CHEIA_MV); /* por isso o PWM é limitado */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_labirinto_le_paredes_dos_dois_lados);
    RUN_TEST(test_robo_no_centro_da_celula_nas_quatro_direcoes);
    RUN_TEST(test_labirinto_rejeita_desenho_malformado);
    RUN_TEST(test_tof_mede_distancia_ate_as_paredes);
    RUN_TEST(test_tof_frontal_centrado_ve_a_parede_da_frente);
    RUN_TEST(test_tof_fora_de_alcance);
    RUN_TEST(test_motores_desabilitados_nao_movem);
    RUN_TEST(test_rodas_no_ar_contam_pulsos_sem_mover_o_robo);
    RUN_TEST(test_encoders_acompanham_o_deslocamento);
    RUN_TEST(test_colisao_trava_robo_e_encoders);
    RUN_TEST(test_giro_no_lugar);
    RUN_TEST(test_meia_volta_no_lugar_bate_no_corredor);
    RUN_TEST(test_bateria_descarrega_e_passa_pelo_divisor);
    RUN_TEST(test_curvas_da_bateria_batem_com_os_limites);
    return UNITY_END();
}
