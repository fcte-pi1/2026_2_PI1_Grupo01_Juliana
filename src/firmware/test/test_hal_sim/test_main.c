#include <string.h>
#include <unity.h>

#include "config/robo.h"
#include "hal_bateria.h"
#include "hal_boot.h"
#include "hal_bt.h"
#include "hal_encoder.h"
#include "hal_led.h"
#include "hal_motor.h"
#include "hal_nvs.h"
#include "hal_tempo.h"
#include "hal_tof.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"

static const char LABIRINTO_2X2[] =
    "+---+---+\n"
    "|       |\n"
    "+   +---+\n"
    "|   |   |\n"
    "+---+---+\n";

static char saida_bt[256];
static size_t tamanho_saida_bt;

static void capturar_bt(const char *dados, size_t tamanho)
{
    memcpy(saida_bt + tamanho_saida_bt, dados, tamanho);
    tamanho_saida_bt += tamanho;
    saida_bt[tamanho_saida_bt] = '\0';
}

void setUp(void)
{
    sim_mundo_iniciar(LABIRINTO_2X2);
    hal_nvs_sim_apagar_tudo();
    hal_bt_sim_definir_saida(NULL);
    tamanho_saida_bt = 0;
}

void tearDown(void) {}

static void test_motores_comecam_parados_e_em_standby(void)
{
    sim_motor_habilitar(true); /* como se tivesse sobrado de antes do reset */
    hal_motor_iniciar();
    TEST_ASSERT_FALSE(sim_motores_habilitados());

    hal_motor_definir(MOTOR_ESQ, 1000);
    hal_motor_definir(MOTOR_DIR, 1000);
    sim_mundo_avancar(500);
    TEST_ASSERT_EQUAL_INT32(0, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(0, hal_encoder_ler(MOTOR_DIR));
}

static void test_motor_habilitado_gira_e_encoder_conta(void)
{
    hal_motor_iniciar();
    hal_motor_habilitar(true);
    hal_motor_definir(MOTOR_ESQ, 400);
    hal_motor_definir(MOTOR_DIR, 400);
    sim_mundo_avancar(300);
    TEST_ASSERT_TRUE(hal_encoder_ler(MOTOR_ESQ) > 0);
    TEST_ASSERT_TRUE(hal_encoder_ler(MOTOR_DIR) > 0);
}

static void test_tof_ausente_e_reportado_com_o_nome(void)
{
    uint16_t mm;
    TEST_ASSERT_TRUE(hal_tof_iniciar());

    sim_definir_tof_ausente(TOF_ESQUERDO, true);
    TEST_ASSERT_FALSE(hal_tof_iniciar());
    TEST_ASSERT_FALSE(hal_tof_ler_mm(TOF_ESQUERDO, &mm));
    TEST_ASSERT_TRUE(hal_tof_ler_mm(TOF_DIREITO, &mm));
    TEST_ASSERT_EQUAL_STRING("tof_esquerdo", hal_tof_nome(TOF_ESQUERDO));
}

static void test_tof_ve_parede_lateral_na_largada(void)
{
    uint16_t esquerda, direita;
    TEST_ASSERT_TRUE(hal_tof_ler_mm(TOF_ESQUERDO, &esquerda));
    TEST_ASSERT_TRUE(hal_tof_ler_mm(TOF_DIREITO, &direita));
    TEST_ASSERT_TRUE(esquerda < 100);
    TEST_ASSERT_TRUE(direita < 100);
}

static void test_relogio_virtual_anda_com_a_simulacao(void)
{
    uint32_t antes = hal_tempo_ms();
    sim_mundo_avancar(250);
    TEST_ASSERT_EQUAL_UINT32(antes + 250, hal_tempo_ms());
}

static void test_bateria_boot_e_led(void)
{
    sim_definir_bateria_mv(7500);
    TEST_ASSERT_UINT16_WITHIN(1, (uint16_t)(7500 / BATERIA_DIVISOR), hal_bateria_ler_adc_mv());

    TEST_ASSERT_FALSE(hal_boot_pressionado());
    sim_definir_boot(true);
    TEST_ASSERT_TRUE(hal_boot_pressionado());

    hal_led_definir(true);
    hal_buzzer_definir(true);
    TEST_ASSERT_TRUE(sim_led_aceso());
    TEST_ASSERT_TRUE(sim_buzzer_ligado());
}

static int vezes_espelho;
static bool ultimo_espelho;

static void espelhar_led(bool aceso)
{
    vezes_espelho++;
    ultimo_espelho = aceso;
}

static void test_led_e_repassado_ao_espelho(void)
{
    vezes_espelho = 0;
    hal_led_sim_definir_espelho(espelhar_led);

    hal_led_definir(true);
    TEST_ASSERT_EQUAL_INT(1, vezes_espelho);
    TEST_ASSERT_TRUE(ultimo_espelho);

    hal_led_definir(false);
    TEST_ASSERT_EQUAL_INT(2, vezes_espelho);
    TEST_ASSERT_FALSE(ultimo_espelho);

    hal_led_sim_definir_espelho(NULL);
    hal_led_definir(true);
    TEST_ASSERT_EQUAL_INT(2, vezes_espelho);
}

static void test_nvs_grava_le_e_sobrevive_ao_reinicio(void)
{
    int gravado[3] = {1, 2, 3};
    int lido[3] = {0};

    TEST_ASSERT_TRUE(hal_nvs_gravar("mapa", gravado, sizeof(gravado)));
    hal_nvs_iniciar(); /* "reboot" */
    TEST_ASSERT_TRUE(hal_nvs_ler("mapa", lido, sizeof(lido)));
    TEST_ASSERT_EQUAL_INT_ARRAY(gravado, lido, 3);

    /* Tamanho diferente do gravado é recusado (registro de outra versão). */
    TEST_ASSERT_FALSE(hal_nvs_ler("mapa", lido, sizeof(int)));
    TEST_ASSERT_FALSE(hal_nvs_ler("nao_existe", lido, sizeof(lido)));

    TEST_ASSERT_TRUE(hal_nvs_apagar("mapa"));
    TEST_ASSERT_FALSE(hal_nvs_ler("mapa", lido, sizeof(lido)));
}

static void test_bt_envia_para_a_saida_e_recebe_injetado(void)
{
    hal_bt_iniciar("teste");
    TEST_ASSERT_FALSE(hal_bt_conectado());
    TEST_ASSERT_EQUAL_size_t(0, hal_bt_escrever("x", 1));

    hal_bt_sim_definir_saida(capturar_bt);
    TEST_ASSERT_TRUE(hal_bt_conectado());
    hal_bt_escrever("{\"tipo\":\"heartbeat\"}\n", 21);
    TEST_ASSERT_EQUAL_STRING("{\"tipo\":\"heartbeat\"}\n", saida_bt);

    TEST_ASSERT_EQUAL_INT(-1, hal_bt_ler_byte());
    hal_bt_sim_injetar("ok");
    TEST_ASSERT_EQUAL_INT('o', hal_bt_ler_byte());
    TEST_ASSERT_EQUAL_INT('k', hal_bt_ler_byte());
    TEST_ASSERT_EQUAL_INT(-1, hal_bt_ler_byte());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_motores_comecam_parados_e_em_standby);
    RUN_TEST(test_motor_habilitado_gira_e_encoder_conta);
    RUN_TEST(test_tof_ausente_e_reportado_com_o_nome);
    RUN_TEST(test_tof_ve_parede_lateral_na_largada);
    RUN_TEST(test_relogio_virtual_anda_com_a_simulacao);
    RUN_TEST(test_bateria_boot_e_led);
    RUN_TEST(test_led_e_repassado_ao_espelho);
    RUN_TEST(test_nvs_grava_le_e_sobrevive_ao_reinicio);
    RUN_TEST(test_bt_envia_para_a_saida_e_recebe_injetado);
    return UNITY_END();
}
