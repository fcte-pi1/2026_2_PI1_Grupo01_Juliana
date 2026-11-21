/*
 * Corrida: máquina de estados + navegação + NVS, com a HAL simulada e o labirinto de
 * demonstração. O que a tarefa de navegação faz com botão e comandos, aqui por eventos.
 */

#include <string.h>
#include <unity.h>

#include "config/robo.h"
#include "corrida.h"
#include "hal_nvs.h"
#include "hal_tof.h"
#include "sensores.h"
#include "sim/hal_sim.h"
#include "sim_mundo.h"
#include "../../src/app/labirinto_demo.h"

#define PERIODO_MS         50
#define CICLOS_POR_DECISAO 4
#define PASSOS_MAX         400

static corrida_t c;
static corrida_efeitos_t ef;
static telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX];

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

static void boot(void)
{
    sim_mundo_iniciar(LABIRINTO_DEMO);
    (void)hal_tof_iniciar();
    iniciar_nvs();
    corrida_iniciar(&c, ROBO_TIPO_LABIRINTO);
    /* O robô liga ocioso: o health-check só roda quando o comando chega. */
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);
    sim_mundo_na_celula(c.nav.x, c.nav.y, (int)c.nav.direcao);
    sensores_iniciar();
}

static void health_check(bool aprovado)
{
    corrida_health_check_concluido(&c, aprovado, ROBO_TIPO_LABIRINTO, &ef);
}

static void largar(void) { corrida_evento(&c, MAQ_EV_CMD_LARGAR, &ef); }

static int decidir(void)
{
    for (int i = 0; i < CICLOS_POR_DECISAO; i++) {
        sim_mundo_avancar(PERIODO_MS);
        sensores_ler();
    }
    paredes_t p = sensores_paredes();
    nav_leitura_t leitura = {p.frente, p.esquerda, p.direita};
    bool reposicionar;
    int n = corrida_decidir(&c, leitura, &reposicionar, saida);
    if (reposicionar) {
        sim_mundo_na_celula(c.nav.x, c.nav.y, (int)c.nav.direcao);
    }
    return n;
}

static void ate_passar_de_uma_celula(void)
{
    for (int i = 0; i < PASSOS_MAX && c.nav.x == 0 && c.nav.y == 0; i++) {
        decidir();
    }
    TEST_ASSERT_TRUE_MESSAGE(c.nav.x != 0 || c.nav.y != 0, "a corrida nao saiu da largada");
}

void setUp(void) { limpar_nvs(); }

void tearDown(void) {}

static void test_depois_do_boot_espera_em_health_check_sem_partir_sozinho(void)
{
    boot();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_EQUAL_INT(0, decidir());
    }
    TEST_ASSERT_EQUAL_INT8(0, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(0, c.nav.y);
    TEST_ASSERT_FALSE(corrida_em_movimento(&c));
}

static void test_health_check_aprovado_continua_esperando_o_largar(void)
{
    boot();
    health_check(true);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());
    TEST_ASSERT_FALSE(corrida_em_movimento(&c));
}

static void test_largar_com_health_check_reprovado_nao_parte(void)
{
    boot();
    health_check(false);
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());
}

static void test_largar_antes_do_resultado_do_health_check_nao_parte(void)
{
    boot();
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());
}

static void test_largar_com_health_check_aprovado_parte_e_publica_passo(void)
{
    boot();
    health_check(true);
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));
    /* A célula da largada não gera passo: o primeiro sai quando a navegação muda de célula. */
    TEST_ASSERT_EQUAL_INT(0, decidir());
    int n = 0;
    for (int i = 0; i < PASSOS_MAX && n == 0; i++) {
        n = decidir();
    }
    TEST_ASSERT_TRUE(n >= 1);
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_PASSO, saida[0].tipo);
}

static void test_interromper_na_corrida_para_e_envia_falha_web_sem_motivo(void)
{
    boot();
    health_check(true);
    largar();
    decidir();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    TEST_ASSERT_TRUE(ef.parar_motores);
    TEST_ASSERT_TRUE(ef.tem_falha);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ORIGEM_WEB, ef.falha.origem);
    TEST_ASSERT_EQUAL_INT(CONTRATO_MOTIVO_NENHUM, ef.falha.motivo);
    TEST_ASSERT_EQUAL_INT(CONTRATO_COMPONENTE_NENHUM, ef.falha.componente);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)c.nav.x, ef.falha.x);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)c.nav.y, ef.falha.y);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, corrida_estado_contrato(&c));
    TEST_ASSERT_FALSE(corrida_em_movimento(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());
}

static void test_interromper_ja_parado_em_failed_repete_a_falha_sem_parar(void)
{
    boot();
    health_check(true);
    largar();
    decidir();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    TEST_ASSERT_TRUE(ef.tem_falha);
    TEST_ASSERT_FALSE(ef.parar_motores);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ORIGEM_WEB, ef.falha.origem);
}

static void test_interromper_sem_corrida_nao_envia_falha(void)
{
    boot();
    health_check(true);
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    TEST_ASSERT_FALSE(ef.tem_falha);
    TEST_ASSERT_FALSE(ef.parar_motores);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
}

static void test_boot_na_corrida_envia_falha_boot_encerrado_pelo_operador(void)
{
    boot();
    health_check(true);
    largar();
    decidir();
    corrida_evento(&c, MAQ_EV_BOOT_TOQUE, &ef);
    TEST_ASSERT_TRUE(ef.parar_motores);
    TEST_ASSERT_TRUE(ef.tem_falha);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ORIGEM_BOOT, ef.falha.origem);
    TEST_ASSERT_EQUAL_INT(CONTRATO_MOTIVO_ENCERRADO_OPERADOR, ef.falha.motivo);
    TEST_ASSERT_EQUAL_INT(CONTRATO_COMPONENTE_NENHUM, ef.falha.componente);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());
}

static void test_interrupcao_grava_failed_na_nvs_e_o_reset_volta_na_mesma_celula(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    int8_t x = c.nav.x, y = c.nav.y;

    boot();
    TEST_ASSERT_EQUAL_INT(MEM_RETOMADA, c.inicio);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_FAILED, c.estado);
    TEST_ASSERT_EQUAL_INT8(x, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(y, c.nav.y);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
}

static void test_largar_depois_da_falha_retoma_da_mesma_celula(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    int8_t x = c.nav.x, y = c.nav.y;

    /* A falha manual não exige novo health-check: o último continua valendo. */
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));
    TEST_ASSERT_FALSE(ef.reposicionar);
    TEST_ASSERT_EQUAL_INT8(x, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(y, c.nav.y);
    TEST_ASSERT_TRUE(decidir() >= 1);
}

static void test_depois_do_reset_o_primeiro_passo_da_retomada_e_publicado(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();

    boot();
    health_check(true);
    largar();
    /* A retomada começa na célula da falha e já publica o próprio passo. */
    TEST_ASSERT_TRUE(decidir() >= 1);
    TEST_ASSERT_EQUAL_INT(TELEMETRIA_DECISAO_PASSO, saida[0].tipo);
}

static void test_comando_health_check_so_vale_parado(void)
{
    boot();
    health_check(true);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);
    TEST_ASSERT_TRUE(ef.rodar_health_check);
    /* Enquanto o novo não termina, o anterior não vale para largar. */
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_HEALTH_CHECK, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());

    health_check(true);
    largar();
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);
    TEST_ASSERT_FALSE(ef.rodar_health_check);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));
}

static void test_boot_curto_parado_apaga_o_mapa_e_reinicia_a_navegacao(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    TEST_ASSERT_TRUE(hal_nvs_existe(MEM_CHAVE_MAPA));

    corrida_evento(&c, MAQ_EV_BOOT_CURTO, &ef);
    TEST_ASSERT_EQUAL_INT(1, ef.bipes);
    TEST_ASSERT_TRUE(ef.reposicionar);
    TEST_ASSERT_FALSE(ef.tem_falha);
    TEST_ASSERT_FALSE(hal_nvs_existe(MEM_CHAVE_MAPA));
    TEST_ASSERT_EQUAL_INT8(0, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(0, c.nav.y);
}

static void test_boot_longo_parado_apaga_mapa_e_tipos_com_dois_bipes(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    TEST_ASSERT_TRUE(hal_nvs_existe(MEM_CHAVE_TIPOS));

    corrida_evento(&c, MAQ_EV_BOOT_LONGO, &ef);
    TEST_ASSERT_EQUAL_INT(2, ef.bipes);
    TEST_ASSERT_FALSE(hal_nvs_existe(MEM_CHAVE_MAPA));
    TEST_ASSERT_FALSE(hal_nvs_existe(MEM_CHAVE_TIPOS));
}

static void test_boot_parado_antes_do_health_check_nao_apaga_nada(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);

    corrida_evento(&c, MAQ_EV_BOOT_LONGO, &ef);
    TEST_ASSERT_EQUAL_INT(0, ef.bipes);
    TEST_ASSERT_TRUE(hal_nvs_existe(MEM_CHAVE_MAPA));
}

static void test_corrida_completa_termina_em_sucesso_e_largar_recomeca_da_largada(void)
{
    boot();
    health_check(true);
    largar();

    bool sucesso = false;
    int sem_saida = 0;
    for (int i = 0; i < PASSOS_MAX && !sucesso; i++) {
        int n = decidir();
        for (int k = 0; k < n; k++) {
            sucesso = sucesso || saida[k].tipo == TELEMETRIA_DECISAO_SUCESSO;
            sem_saida += saida[k].tipo == TELEMETRIA_DECISAO_SEM_SAIDA;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sucesso, "a corrida nao chegou ao objetivo");
    TEST_ASSERT_EQUAL_INT(0, sem_saida);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_SUCCESS, corrida_estado_contrato(&c));
    TEST_ASSERT_FALSE(corrida_em_movimento(&c));
    TEST_ASSERT_EQUAL_INT(0, decidir());

    largar();
    TEST_ASSERT_TRUE(ef.reposicionar);
    TEST_ASSERT_EQUAL_INT(CONTRATO_ESTADO_RUNNING, corrida_estado_contrato(&c));
    TEST_ASSERT_EQUAL_INT8(0, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(0, c.nav.y);
}

static void test_dip_diferente_depois_do_health_check_reinicia_a_navegacao(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);

    corrida_health_check_concluido(&c, true, NAV_TIPO_8X4, &ef);
    TEST_ASSERT_TRUE(ef.reposicionar);
    TEST_ASSERT_EQUAL_INT(NAV_TIPO_8X4, c.tipo);
    TEST_ASSERT_EQUAL_INT(NAV_TIPO_8X4, c.nav.tipo);
    TEST_ASSERT_EQUAL_INT8(0, c.nav.x);
    TEST_ASSERT_EQUAL_INT8(0, c.nav.y);
}

static void test_dip_igual_depois_do_health_check_mantem_a_navegacao(void)
{
    boot();
    health_check(true);
    largar();
    ate_passar_de_uma_celula();
    corrida_evento(&c, MAQ_EV_CMD_INTERROMPER, &ef);
    corrida_evento(&c, MAQ_EV_CMD_HEALTH_CHECK, &ef);
    int8_t x = c.nav.x;

    corrida_health_check_concluido(&c, true, ROBO_TIPO_LABIRINTO, &ef);
    TEST_ASSERT_FALSE(ef.reposicionar);
    TEST_ASSERT_EQUAL_INT8(x, c.nav.x);
}

#ifdef ARDUINO
/* Na ESP32 o ponto de entrada é o setup() de entrada_arduino.cpp. */
int executar_testes(void)
#else
int main(void)
#endif
{
    UNITY_BEGIN();
    RUN_TEST(test_depois_do_boot_espera_em_health_check_sem_partir_sozinho);
    RUN_TEST(test_health_check_aprovado_continua_esperando_o_largar);
    RUN_TEST(test_largar_com_health_check_reprovado_nao_parte);
    RUN_TEST(test_largar_antes_do_resultado_do_health_check_nao_parte);
    RUN_TEST(test_largar_com_health_check_aprovado_parte_e_publica_passo);
    RUN_TEST(test_interromper_na_corrida_para_e_envia_falha_web_sem_motivo);
    RUN_TEST(test_interromper_ja_parado_em_failed_repete_a_falha_sem_parar);
    RUN_TEST(test_interromper_sem_corrida_nao_envia_falha);
    RUN_TEST(test_boot_na_corrida_envia_falha_boot_encerrado_pelo_operador);
    RUN_TEST(test_interrupcao_grava_failed_na_nvs_e_o_reset_volta_na_mesma_celula);
    RUN_TEST(test_largar_depois_da_falha_retoma_da_mesma_celula);
    RUN_TEST(test_depois_do_reset_o_primeiro_passo_da_retomada_e_publicado);
    RUN_TEST(test_comando_health_check_so_vale_parado);
    RUN_TEST(test_boot_curto_parado_apaga_o_mapa_e_reinicia_a_navegacao);
    RUN_TEST(test_boot_longo_parado_apaga_mapa_e_tipos_com_dois_bipes);
    RUN_TEST(test_boot_parado_antes_do_health_check_nao_apaga_nada);
    RUN_TEST(test_corrida_completa_termina_em_sucesso_e_largar_recomeca_da_largada);
    RUN_TEST(test_dip_diferente_depois_do_health_check_reinicia_a_navegacao);
    RUN_TEST(test_dip_igual_depois_do_health_check_mantem_a_navegacao);
    return UNITY_END();
}
