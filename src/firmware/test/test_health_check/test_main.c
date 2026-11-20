/*
 * Health-check: os 8 itens, a reprovação que nomeia
 * o componente e a execução pela HAL simulada.
 */

#include <unity.h>

#include "config/robo.h"
#include "hal_encoder.h"
#include "health_check.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"

/* (0, 0) com parede dos dois lados e a frente aberta: os 3 ToF medem algo. */
static const char LABIRINTO_4X2[] =
    "+---+---+---+---+\n"
    "|   |           |\n"
    "+   +   +   +---+\n"
    "|   |           |\n"
    "+---+---+---+---+\n";

#define BATERIA_BOA_MV 8000

static bateria_t bateria;
static hc_resultado_t r;

/* Na simulação, esperar é deixar o mundo andar (as rodas giram com a potência dada). */
static void esperar_sim(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 5) {
        sim_mundo_avancar(5);
    }
}

void setUp(void)
{
    sim_mundo_iniciar(LABIRINTO_4X2);
    sim_definir_rodas_no_ar(true); /* como no autoteste real: rodas no ar */
    hal_dip_sim_definir(0);
    bateria_iniciar(&bateria);
}

void tearDown(void) {}

static tof_autoteste_t tof_ok(const char *nome, uint16_t mm)
{
    tof_autoteste_t t = {nome, true, true, mm};
    return t;
}

#define DIP_4X4 CONTRATO_DIP_4X4

static void tofs_ok(tof_autoteste_t tof[TOF_QTD])
{
    tof[TOF_FRONTAL] = tof_ok("tof_frontal", 300);
    tof[TOF_ESQUERDO] = tof_ok("tof_esquerdo", 77);
    tof[TOF_DIREITO] = tof_ok("tof_direito", 77);
}

/* ---------- parte pura ---------- */

static const int32_t pulsos_ok[MOTOR_QTD] = {400, 380};

void test_tudo_aprovado_gera_os_8_itens_na_ordem_do_adr(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    int32_t pulsos[MOTOR_QTD] = {400, 380};
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos, DIP_4X4, &r);

    TEST_ASSERT_TRUE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_NENHUM, r.reprovado);
    contrato_componente_t ordem[HC_ITENS] = {
        CONTRATO_COMPONENTE_BATERIA,         CONTRATO_COMPONENTE_TOF_FRONTAL,
        CONTRATO_COMPONENTE_TOF_ESQUERDO,    CONTRATO_COMPONENTE_TOF_DIREITO,
        CONTRATO_COMPONENTE_MOTOR_ESQUERDO,  CONTRATO_COMPONENTE_MOTOR_DIREITO,
        CONTRATO_COMPONENTE_ENCODER_ESQUERDO, CONTRATO_COMPONENTE_ENCODER_DIREITO};
    for (int i = 0; i < HC_ITENS; i++) {
        TEST_ASSERT_EQUAL(ordem[i], r.item[i].componente);
        TEST_ASSERT_TRUE(r.item[i].aprovado);
    }
}

void test_valores_dos_itens_seguem_o_contrato(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    int32_t pulsos[MOTOR_QTD] = {400, 380};
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos, DIP_4X4, &r);

    TEST_ASSERT_EQUAL_INT32(7800, r.item[0].valor);
    TEST_ASSERT_TRUE(r.item[0].tem_valor);
    TEST_ASSERT_EQUAL_INT32(300, r.item[1].valor);
    TEST_ASSERT_FALSE(r.item[4].tem_valor); /* motores: valor null */
    TEST_ASSERT_FALSE(r.item[5].tem_valor);
    TEST_ASSERT_EQUAL_INT32(400, r.item[6].valor);
    TEST_ASSERT_EQUAL_INT32(380, r.item[7].valor);
}

void test_tof_reprovado_reprova_o_resultado_e_nomeia_o_componente(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    tof[TOF_ESQUERDO].aprovado = false;
    tof[TOF_ESQUERDO].tem_valor = false;
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos_ok, DIP_4X4, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_TOF_ESQUERDO, r.reprovado);
    TEST_ASSERT_FALSE(r.item[2].aprovado);
    TEST_ASSERT_FALSE(r.item[2].tem_valor);
    TEST_ASSERT_TRUE(r.item[1].aprovado);
    TEST_ASSERT_TRUE(r.item[3].aprovado);
}

void test_bateria_baixa_reprova_com_o_valor_medido(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    hc_avaliar(6500, BATERIA_BAIXA, tof, pulsos_ok, DIP_4X4, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_BATERIA, r.reprovado);
    TEST_ASSERT_TRUE(r.item[0].tem_valor);
    TEST_ASSERT_EQUAL_INT32(6500, r.item[0].valor);
}

void test_bateria_em_alerta_ainda_aprova(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    hc_avaliar(7200, BATERIA_ALERTA, tof, pulsos_ok, DIP_4X4, &r);
    TEST_ASSERT_TRUE(r.aprovado);
}

void test_bateria_sem_leitura_reprova_com_valor_nulo(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    hc_avaliar(0, BATERIA_NORMAL, tof, pulsos_ok, DIP_4X4, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_FALSE(r.item[0].aprovado);
    TEST_ASSERT_FALSE(r.item[0].tem_valor);
}

void test_varios_reprovados_nomeiam_o_primeiro_da_lista_e_marcam_todos(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    tof[TOF_FRONTAL].aprovado = false;
    tof[TOF_DIREITO].aprovado = false;
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos_ok, DIP_4X4, &r);

    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_TOF_FRONTAL, r.reprovado);
    TEST_ASSERT_FALSE(r.item[1].aprovado);
    TEST_ASSERT_FALSE(r.item[3].aprovado);
    TEST_ASSERT_TRUE(r.item[2].aprovado);
}

void test_roda_sem_pulsos_reprova_motor_e_encoder_da_mesma_roda(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    int32_t pulsos[MOTOR_QTD] = {400, 0};
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos, DIP_4X4, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_MOTOR_DIREITO, r.reprovado);
    TEST_ASSERT_TRUE(r.item[4].aprovado);  /* motor esquerdo */
    TEST_ASSERT_FALSE(r.item[5].aprovado); /* motor direito */
    TEST_ASSERT_TRUE(r.item[6].aprovado);  /* encoder esquerdo */
    TEST_ASSERT_FALSE(r.item[7].aprovado); /* encoder direito */
    TEST_ASSERT_EQUAL_INT32(0, r.item[7].valor);
}

void test_pulsos_no_limite_aprovam_e_um_abaixo_reprova(void)
{
    TEST_ASSERT_TRUE(motor_autoteste_aprovado(MOTOR_AUTOTESTE_MIN_PULSOS));
    TEST_ASSERT_FALSE(motor_autoteste_aprovado(MOTOR_AUTOTESTE_MIN_PULSOS - 1));
}

void test_dip_valido_vai_no_resultado(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos_ok, CONTRATO_DIP_12X4, &r);
    TEST_ASSERT_TRUE(r.aprovado);
    TEST_ASSERT_TRUE(r.dip_valido);
    TEST_ASSERT_EQUAL(CONTRATO_DIP_12X4, r.tipo_dip);
}

void test_dip_invalido_reprova_sem_nomear_componente(void)
{
    tof_autoteste_t tof[TOF_QTD];
    tofs_ok(tof);
    hc_avaliar(7800, BATERIA_NORMAL, tof, pulsos_ok, CONTRATO_DIP_INVALIDO, &r);
    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_FALSE(r.dip_valido);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_NENHUM, r.reprovado);
    for (int i = 0; i < HC_ITENS; i++) {
        TEST_ASSERT_TRUE(r.item[i].aprovado);
    }
}

/* ---------- pela HAL simulada ---------- */

void test_executar_com_a_hal_simulada_aprova_tudo(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    hc_executar(&bateria, esperar_sim, &r);

    TEST_ASSERT_TRUE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_NENHUM, r.reprovado);
    TEST_ASSERT_TRUE(r.item[0].tem_valor);
}

void test_executar_com_tof_ausente_reprova_e_nomeia_o_componente(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    sim_definir_tof_ausente(TOF_DIREITO, true);
    hc_executar(&bateria, esperar_sim, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_TOF_DIREITO, r.reprovado);
    TEST_ASSERT_FALSE(r.item[3].aprovado);
    TEST_ASSERT_TRUE(r.item[2].aprovado);
}

void test_executar_com_bateria_baixa_reprova(void)
{
    /* 4,5 V reprova como LiPo (corte 7,0 V) e como pilhas (corte 5,0 V). */
    sim_definir_bateria_mv(4500);
    hc_executar(&bateria, esperar_sim, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_BATERIA, r.reprovado);
}

void test_executar_gira_as_rodas_e_leva_os_pulsos_reais_dos_encoders(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    hc_executar(&bateria, esperar_sim, &r);

    TEST_ASSERT_TRUE(r.aprovado);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(MOTOR_AUTOTESTE_MIN_PULSOS, r.item[6].valor);
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(MOTOR_AUTOTESTE_MIN_PULSOS, r.item[7].valor);
}

void test_executar_com_encoder_travado_reprova_motor_e_encoder_daquela_roda(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    sim_travar_encoder(MOTOR_ESQ, true);
    hc_executar(&bateria, esperar_sim, &r);

    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_COMPONENTE_MOTOR_ESQUERDO, r.reprovado);
    TEST_ASSERT_FALSE(r.item[4].aprovado);
    TEST_ASSERT_FALSE(r.item[6].aprovado);
    TEST_ASSERT_TRUE(r.item[5].aprovado);
    TEST_ASSERT_TRUE(r.item[7].aprovado);
}

void test_executar_deixa_a_potencia_zerada_nas_duas_rodas(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    hc_executar(&bateria, esperar_sim, &r);
    esperar_sim(300); /* a roda simulada ainda desacelera um pouco depois do comando zero */

    int32_t antes_e = hal_encoder_ler(MOTOR_ESQ);
    int32_t antes_d = hal_encoder_ler(MOTOR_DIR);
    esperar_sim(500);
    TEST_ASSERT_EQUAL_INT32(antes_e, hal_encoder_ler(MOTOR_ESQ));
    TEST_ASSERT_EQUAL_INT32(antes_d, hal_encoder_ler(MOTOR_DIR));
}

void test_executar_le_o_dip_da_placa(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    hal_dip_sim_definir(0x1);
    hc_executar(&bateria, esperar_sim, &r);
    TEST_ASSERT_EQUAL(CONTRATO_DIP_8X4, r.tipo_dip);

    hal_dip_sim_definir(0x3);
    hc_executar(&bateria, esperar_sim, &r);
    TEST_ASSERT_FALSE(r.aprovado);
    TEST_ASSERT_EQUAL(CONTRATO_DIP_INVALIDO, r.tipo_dip);
}

void test_executar_de_novo_depois_de_consertar_aprova(void)
{
    sim_definir_bateria_mv(BATERIA_BOA_MV);
    sim_definir_tof_ausente(TOF_ESQUERDO, true);
    hc_executar(&bateria, esperar_sim, &r);
    TEST_ASSERT_FALSE(r.aprovado);

    sim_definir_tof_ausente(TOF_ESQUERDO, false);
    hc_executar(&bateria, esperar_sim, &r);
    TEST_ASSERT_TRUE(r.aprovado);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tudo_aprovado_gera_os_8_itens_na_ordem_do_adr);
    RUN_TEST(test_valores_dos_itens_seguem_o_contrato);
    RUN_TEST(test_tof_reprovado_reprova_o_resultado_e_nomeia_o_componente);
    RUN_TEST(test_bateria_baixa_reprova_com_o_valor_medido);
    RUN_TEST(test_bateria_em_alerta_ainda_aprova);
    RUN_TEST(test_bateria_sem_leitura_reprova_com_valor_nulo);
    RUN_TEST(test_varios_reprovados_nomeiam_o_primeiro_da_lista_e_marcam_todos);
    RUN_TEST(test_roda_sem_pulsos_reprova_motor_e_encoder_da_mesma_roda);
    RUN_TEST(test_pulsos_no_limite_aprovam_e_um_abaixo_reprova);
    RUN_TEST(test_dip_valido_vai_no_resultado);
    RUN_TEST(test_dip_invalido_reprova_sem_nomear_componente);
    RUN_TEST(test_executar_com_a_hal_simulada_aprova_tudo);
    RUN_TEST(test_executar_gira_as_rodas_e_leva_os_pulsos_reais_dos_encoders);
    RUN_TEST(test_executar_com_encoder_travado_reprova_motor_e_encoder_daquela_roda);
    RUN_TEST(test_executar_deixa_a_potencia_zerada_nas_duas_rodas);
    RUN_TEST(test_executar_le_o_dip_da_placa);
    RUN_TEST(test_executar_com_tof_ausente_reprova_e_nomeia_o_componente);
    RUN_TEST(test_executar_com_bateria_baixa_reprova);
    RUN_TEST(test_executar_de_novo_depois_de_consertar_aprova);
    return UNITY_END();
}
