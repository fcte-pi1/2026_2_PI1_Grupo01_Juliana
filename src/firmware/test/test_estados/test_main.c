/*
 * Máquina de estados — FIRM-09 (#138). Cada teste segue um cenário da spec:
 * health-check, largada com o último health-check aprovado, comandos e BOOT.
 */

#include <unity.h>

#include "maquina.h"

static maq_t m;

void setUp(void)
{
    maq_iniciar(&m);
}

void tearDown(void) {}

static void ate_aguardando(void)
{
    maq_evento(&m, MAQ_EV_HC_APROVADO);
}

static void ate_running(void)
{
    ate_aguardando();
    maq_evento(&m, MAQ_EV_CMD_LARGAR);
}

void test_boot_comeca_no_health_check_e_nao_anda(void)
{
    TEST_ASSERT_EQUAL(MAQ_HEALTH_CHECK, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_HEALTH_CHECK, maq_estado_contrato(&m));
    TEST_ASSERT_FALSE(maq_andando(&m));
}

void test_health_check_aprovado_espera_a_largada(void)
{
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_HC_APROVADO));
    TEST_ASSERT_EQUAL(MAQ_AGUARDANDO, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_HEALTH_CHECK, maq_estado_contrato(&m));
    TEST_ASSERT_FALSE(maq_andando(&m));
}

void test_health_check_reprovado_nao_parte_com_largar(void)
{
    maq_evento(&m, MAQ_EV_HC_REPROVADO);
    TEST_ASSERT_EQUAL(MAQ_REPROVADO, m.estado);
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_LARGAR));
    TEST_ASSERT_EQUAL(MAQ_REPROVADO, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_HEALTH_CHECK, maq_estado_contrato(&m));
}

void test_largar_antes_do_resultado_do_health_check_e_ignorado(void)
{
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_LARGAR));
    TEST_ASSERT_EQUAL(MAQ_HEALTH_CHECK, m.estado);
}

void test_largar_com_health_check_aprovado_parte(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARTIR, maq_evento(&m, MAQ_EV_CMD_LARGAR));
    TEST_ASSERT_EQUAL(MAQ_RUNNING, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_RUNNING, maq_estado_contrato(&m));
    TEST_ASSERT_TRUE(maq_andando(&m));
}

void test_largar_na_corrida_nao_faz_nada(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_LARGAR));
    TEST_ASSERT_EQUAL(MAQ_RUNNING, m.estado);
}

void test_health_check_pedido_parado_roda_de_novo(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_RODAR_HEALTH_CHECK, maq_evento(&m, MAQ_EV_CMD_HEALTH_CHECK));
    TEST_ASSERT_EQUAL(MAQ_HEALTH_CHECK, m.estado);
    /* Enquanto o novo não termina, o anterior não vale para largar. */
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_LARGAR));
}

void test_health_check_pedido_depois_de_reprovado_pode_aprovar(void)
{
    maq_evento(&m, MAQ_EV_HC_REPROVADO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_RODAR_HEALTH_CHECK, maq_evento(&m, MAQ_EV_CMD_HEALTH_CHECK));
    maq_evento(&m, MAQ_EV_HC_APROVADO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARTIR, maq_evento(&m, MAQ_EV_CMD_LARGAR));
}

void test_health_check_pedido_na_corrida_e_ignorado(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_HEALTH_CHECK));
    TEST_ASSERT_EQUAL(MAQ_RUNNING, m.estado);
}

void test_health_check_pedido_em_success_e_em_failed_roda(void)
{
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_SUCESSO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_RODAR_HEALTH_CHECK, maq_evento(&m, MAQ_EV_CMD_HEALTH_CHECK));

    maq_iniciar(&m);
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_FALHA);
    TEST_ASSERT_EQUAL(MAQ_ACAO_RODAR_HEALTH_CHECK, maq_evento(&m, MAQ_EV_CMD_HEALTH_CHECK));
}

void test_resultado_do_health_check_fora_dele_e_ignorado(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_HC_REPROVADO));
    TEST_ASSERT_EQUAL(MAQ_RUNNING, m.estado);
}

void test_corrida_termina_em_success_ou_failed(void)
{
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_SUCESSO);
    TEST_ASSERT_EQUAL(MAQ_SUCCESS, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_SUCCESS, maq_estado_contrato(&m));
    TEST_ASSERT_FALSE(maq_andando(&m));

    maq_iniciar(&m);
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_FALHA);
    TEST_ASSERT_EQUAL(MAQ_FAILED, m.estado);
    TEST_ASSERT_EQUAL(CONTRATO_ESTADO_FAILED, maq_estado_contrato(&m));
    TEST_ASSERT_FALSE(maq_andando(&m));
}

void test_retomar_depois_de_failed_parte_com_o_ultimo_health_check_aprovado(void)
{
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_FALHA);
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARTIR, maq_evento(&m, MAQ_EV_CMD_LARGAR));
    TEST_ASSERT_EQUAL(MAQ_RUNNING, m.estado);
}

void test_nova_execucao_depois_de_success_parte(void)
{
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_SUCESSO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARTIR, maq_evento(&m, MAQ_EV_CMD_LARGAR));
}

void test_interromper_na_corrida_para_com_falha_da_web(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARAR_FALHA_WEB, maq_evento(&m, MAQ_EV_CMD_INTERROMPER));
    TEST_ASSERT_EQUAL(MAQ_FAILED, m.estado);
}

void test_interromper_ja_parado_so_repete_a_confirmacao(void)
{
    ate_running();
    maq_evento(&m, MAQ_EV_CMD_INTERROMPER);
    TEST_ASSERT_EQUAL(MAQ_ACAO_REPETIR_FALHA_WEB, maq_evento(&m, MAQ_EV_CMD_INTERROMPER));
    TEST_ASSERT_EQUAL(MAQ_FAILED, m.estado);
}

void test_interromper_fora_de_tentativa_e_ignorado(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_CMD_INTERROMPER));
    TEST_ASSERT_EQUAL(MAQ_AGUARDANDO, m.estado);
}

void test_boot_na_corrida_para_com_falha_do_boot(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARAR_FALHA_BOOT, maq_evento(&m, MAQ_EV_BOOT_TOQUE));
    TEST_ASSERT_EQUAL(MAQ_FAILED, m.estado);
}

void test_boot_curto_ou_longo_na_corrida_tambem_para(void)
{
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARAR_FALHA_BOOT, maq_evento(&m, MAQ_EV_BOOT_CURTO));

    maq_iniciar(&m);
    ate_running();
    TEST_ASSERT_EQUAL(MAQ_ACAO_PARAR_FALHA_BOOT, maq_evento(&m, MAQ_EV_BOOT_LONGO));
}

void test_boot_curto_parado_apaga_o_mapa(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_APAGAR_MAPA, maq_evento(&m, MAQ_EV_BOOT_CURTO));
    TEST_ASSERT_EQUAL(MAQ_AGUARDANDO, m.estado);
}

void test_boot_longo_parado_apaga_tudo(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_APAGAR_TUDO, maq_evento(&m, MAQ_EV_BOOT_LONGO));
    TEST_ASSERT_EQUAL(MAQ_AGUARDANDO, m.estado);
}

void test_boot_parado_apaga_tambem_depois_de_reprovado_success_e_failed(void)
{
    maq_evento(&m, MAQ_EV_HC_REPROVADO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_APAGAR_MAPA, maq_evento(&m, MAQ_EV_BOOT_CURTO));

    maq_iniciar(&m);
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_SUCESSO);
    TEST_ASSERT_EQUAL(MAQ_ACAO_APAGAR_TUDO, maq_evento(&m, MAQ_EV_BOOT_LONGO));

    maq_iniciar(&m);
    ate_running();
    maq_evento(&m, MAQ_EV_CORRIDA_FALHA);
    TEST_ASSERT_EQUAL(MAQ_ACAO_APAGAR_MAPA, maq_evento(&m, MAQ_EV_BOOT_CURTO));
}

void test_boot_durante_o_health_check_e_ignorado(void)
{
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_BOOT_CURTO));
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_BOOT_LONGO));
    TEST_ASSERT_EQUAL(MAQ_HEALTH_CHECK, m.estado);
}

void test_toque_simples_parado_nao_apaga_nada(void)
{
    ate_aguardando();
    TEST_ASSERT_EQUAL(MAQ_ACAO_NENHUMA, maq_evento(&m, MAQ_EV_BOOT_TOQUE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_comeca_no_health_check_e_nao_anda);
    RUN_TEST(test_health_check_aprovado_espera_a_largada);
    RUN_TEST(test_health_check_reprovado_nao_parte_com_largar);
    RUN_TEST(test_largar_antes_do_resultado_do_health_check_e_ignorado);
    RUN_TEST(test_largar_com_health_check_aprovado_parte);
    RUN_TEST(test_largar_na_corrida_nao_faz_nada);
    RUN_TEST(test_health_check_pedido_parado_roda_de_novo);
    RUN_TEST(test_health_check_pedido_depois_de_reprovado_pode_aprovar);
    RUN_TEST(test_health_check_pedido_na_corrida_e_ignorado);
    RUN_TEST(test_health_check_pedido_em_success_e_em_failed_roda);
    RUN_TEST(test_resultado_do_health_check_fora_dele_e_ignorado);
    RUN_TEST(test_corrida_termina_em_success_ou_failed);
    RUN_TEST(test_retomar_depois_de_failed_parte_com_o_ultimo_health_check_aprovado);
    RUN_TEST(test_nova_execucao_depois_de_success_parte);
    RUN_TEST(test_interromper_na_corrida_para_com_falha_da_web);
    RUN_TEST(test_interromper_ja_parado_so_repete_a_confirmacao);
    RUN_TEST(test_interromper_fora_de_tentativa_e_ignorado);
    RUN_TEST(test_boot_na_corrida_para_com_falha_do_boot);
    RUN_TEST(test_boot_curto_ou_longo_na_corrida_tambem_para);
    RUN_TEST(test_boot_curto_parado_apaga_o_mapa);
    RUN_TEST(test_boot_longo_parado_apaga_tudo);
    RUN_TEST(test_boot_parado_apaga_tambem_depois_de_reprovado_success_e_failed);
    RUN_TEST(test_boot_durante_o_health_check_e_ignorado);
    RUN_TEST(test_toque_simples_parado_nao_apaga_nada);
    return UNITY_END();
}
