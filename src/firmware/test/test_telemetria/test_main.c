/*
 * Telemetria — FIRM-07 (#123). Buffer de 64 eventos com cursor de envio e
 * carimbo no primeiro envio; telemetria_passo com relógio fictício e o link
 * da HAL simulada.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "hal_bt.h"
#include "hal_nvs.h"
#include "sim/hal_sim.h"

#include "telemetria_envio.h"

#define BOOT 7

#define MAX_ESCRITAS 400

static telemetria_buffer_t buffer;
static uint32_t seq;

/* Cada hal_bt_escrever vira uma escrita, com o relógio fictício do momento. */
typedef struct {
    char texto[CONTRATO_TAMANHO_MAXIMO + 1];
    size_t tamanho;
    uint32_t t;
} escrita_t;

static escrita_t escritas[MAX_ESCRITAS];
static int qtd_escritas;
static uint32_t relogio;

static void capturar(const char *dados, size_t tamanho)
{
    TEST_ASSERT_LESS_THAN_INT(MAX_ESCRITAS, qtd_escritas);
    escrita_t *e = &escritas[qtd_escritas++];
    memcpy(e->texto, dados, tamanho);
    e->texto[tamanho] = '\0';
    e->tamanho = tamanho;
    e->t = relogio;
}

void setUp(void)
{
    telemetria_buffer_iniciar(&buffer);
    seq = 0;
    telemetria_iniciar(BOOT);
    hal_bt_iniciar("teste"); /* esvazia a entrada simulada */
    hal_bt_sim_definir_saida(capturar);
    hal_bt_sim_definir_conectado(true);
    hal_bt_sim_limitar_escrita(SIZE_MAX);
    qtd_escritas = 0;
    relogio = 0;
}

void tearDown(void) {}

/* Um passo cuja célula x identifica o evento. */
static void inserir_passo(uint8_t x)
{
    telemetria_evento_t e = {.tipo = TELEMETRIA_EVENTO_PASSO, .m.passo = {.x = x, .y = 0}};
    telemetria_buffer_inserir(&buffer, &e);
}

/* Envia o evento do cursor e avança; devolve o tamanho da linha. */
static int enviar(char *linha, uint32_t agora_ms)
{
    int n = telemetria_buffer_serializar(&buffer, linha, CONTRATO_TAMANHO_MAXIMO + 1, BOOT, &seq,
                                         agora_ms);
    TEST_ASSERT_GREATER_THAN_INT(0, n);
    telemetria_buffer_avancar(&buffer);
    return n;
}

static void test_ordem_de_saida_igual_a_de_insercao(void)
{
    inserir_passo(1);
    inserir_passo(2);
    inserir_passo(3);

    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    char esperado[CONTRATO_TAMANHO_MAXIMO + 1];
    for (uint8_t x = 1; x <= 3; x++) {
        enviar(linha, 100u * x);
        contrato_cabecalho_t cab = {.boot = BOOT, .seq = x - 1u, .t_ms = 100u * x};
        contrato_passo_t passo = {.x = x, .y = 0};
        contrato_passo(esperado, sizeof esperado, &cab, &passo);
        TEST_ASSERT_EQUAL_STRING(esperado, linha);
    }
    TEST_ASSERT_FALSE(telemetria_buffer_pendente(&buffer));
}

static void test_70_insercoes_guardam_as_64_mais_novas(void)
{
    for (uint8_t x = 0; x < 70; x++) {
        inserir_passo(x);
    }
    TEST_ASSERT_EQUAL_UINT32(70, buffer.total);
    TEST_ASSERT_EQUAL_UINT32(TELEMETRIA_BUFFER_EVENTOS, buffer.qtd);
    TEST_ASSERT_EQUAL_UINT32(6, buffer.cursor);
    TEST_ASSERT_EQUAL_UINT32(6, buffer.descartados);

    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    for (uint8_t x = 6; x < 70; x++) {
        enviar(linha, 0);
        TEST_ASSERT_EQUAL_UINT8(x, buffer.eventos[(buffer.cursor - 1) % 64].m.passo.x);
    }
    TEST_ASSERT_FALSE(telemetria_buffer_pendente(&buffer));
}

static void test_voltar_o_cursor_reenvia_as_mesmas_linhas(void)
{
    char primeiro[3][CONTRATO_TAMANHO_MAXIMO + 1];
    char segundo[3][CONTRATO_TAMANHO_MAXIMO + 1];
    int tam[3];
    for (uint8_t x = 0; x < 3; x++) {
        inserir_passo(x);
    }
    for (int i = 0; i < 3; i++) {
        tam[i] = enviar(primeiro[i], 50u * (uint32_t)i);
    }

    telemetria_buffer_rebobinar(&buffer);
    TEST_ASSERT_TRUE(telemetria_buffer_pendente(&buffer));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(tam[i], enviar(segundo[i], 9000));
        TEST_ASSERT_EQUAL_MEMORY(primeiro[i], segundo[i], (size_t)tam[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(3, seq);
}

static void test_descartado_sem_sair_nao_consome_seq(void)
{
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    inserir_passo(0);
    enviar(linha, 0); /* seq 0 */

    /* 64 novos: o já enviado e o primeiro dos novos (nunca enviado) saem. */
    for (uint8_t x = 1; x <= 65; x++) {
        inserir_passo(x);
    }
    TEST_ASSERT_EQUAL_UINT32(2, buffer.cursor);
    TEST_ASSERT_EQUAL_UINT32(2, buffer.carimbados_ate);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.descartados);

    enviar(linha, 0);
    TEST_ASSERT_EQUAL_UINT32(1, buffer.eventos[2 % 64].cab.seq);
    TEST_ASSERT_EQUAL_UINT8(2, buffer.eventos[2 % 64].m.passo.x);
}

static void test_carimbado_descartado_leva_carimbados_ate_junto(void)
{
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    for (uint8_t x = 0; x < 64; x++) {
        inserir_passo(x);
    }
    enviar(linha, 0);
    enviar(linha, 0);
    telemetria_buffer_rebobinar(&buffer);

    inserir_passo(64); /* descarta o 0, já carimbado */
    TEST_ASSERT_EQUAL_UINT32(1, buffer.cursor);
    TEST_ASSERT_EQUAL_UINT32(2, buffer.carimbados_ate);
    TEST_ASSERT_EQUAL_UINT32(0, buffer.descartados);

    enviar(linha, 500); /* reenvio do 1 com o carimbo guardado */
    TEST_ASSERT_EQUAL_UINT32(1, buffer.eventos[1].cab.seq);
    TEST_ASSERT_EQUAL_UINT32(0, buffer.eventos[1].cab.t_ms);
    enviar(linha, 500); /* o 2 recebe o próximo seq */
    TEST_ASSERT_EQUAL_UINT32(2, buffer.eventos[2].cab.seq);
    TEST_ASSERT_EQUAL_UINT32(3, seq);
}

static void test_cada_tipo_usa_o_serializador_do_contrato(void)
{
    telemetria_evento_t eventos[] = {
        {.tipo = TELEMETRIA_EVENTO_HC_ITEM,
         .m.hc_item = {.componente = CONTRATO_COMPONENTE_BATERIA, .aprovado = true,
                       .tem_valor = true, .valor = 8000}},
        {.tipo = TELEMETRIA_EVENTO_HC_RESULTADO,
         .m.hc_resultado = {.aprovado = true, .tipo_dip = CONTRATO_DIP_8X4}},
        {.tipo = TELEMETRIA_EVENTO_FALHA,
         .m.falha = {.motivo = CONTRATO_MOTIVO_STUCK, .x = 2, .y = 1}},
        {.tipo = TELEMETRIA_EVENTO_SUCESSO, .m.sucesso = {.x = 3, .y = 3}},
    };
    const char *tipos[] = {"\"hc_item\"", "\"hc_resultado\"", "\"falha\"", "\"sucesso\""};
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    for (int i = 0; i < 4; i++) {
        telemetria_buffer_inserir(&buffer, &eventos[i]);
        enviar(linha, 0);
        TEST_ASSERT_NOT_NULL(strstr(linha, tipos[i]));
    }
}

static void test_sem_evento_pendente_nao_serializa(void)
{
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    TEST_ASSERT_EQUAL_INT(-1, telemetria_buffer_serializar(&buffer, linha, sizeof linha, BOOT,
                                                           &seq, 0));
    TEST_ASSERT_EQUAL_UINT32(0, seq);
}

/* ---------- telemetria_passo ---------- */

static const telemetria_amostra_t EM_MOVIMENTO = {
    .tel = {.estado = CONTRATO_ESTADO_RUNNING, .x = 1, .y = 2, .bat_mv = 7800},
    .em_movimento = true};
static const telemetria_amostra_t PARADO = {
    .tel = {.estado = CONTRATO_ESTADO_HEALTH_CHECK, .bat_mv = 7800}, .em_movimento = false};

/* Roda as voltas de [de, ate), saltando o tempo que telemetria_passo pede. */
static void rodar(uint32_t de, uint32_t ate, const telemetria_amostra_t *amostra)
{
    for (relogio = de; relogio < ate;) {
        uint32_t espera = telemetria_passo(relogio, amostra);
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(TELEMETRIA_PERIODO_EVENTO_MS, espera);
        relogio += espera > 0 ? espera : 1;
    }
}

static void inserir_evento_passo(uint8_t x)
{
    telemetria_evento_t e = {.tipo = TELEMETRIA_EVENTO_PASSO, .m.passo = {.x = x, .y = 0}};
    telemetria_inserir_evento(&e);
}

static bool eh_tipo(const escrita_t *e, const char *tipo)
{
    char chave[32];
    snprintf(chave, sizeof chave, "\"tipo\":\"%s\"", tipo);
    return strstr(e->texto, chave) != NULL;
}

static uint32_t campo(const escrita_t *e, const char *nome)
{
    char chave[32];
    snprintf(chave, sizeof chave, "\"%s\":", nome);
    const char *p = strstr(e->texto, chave);
    TEST_ASSERT_NOT_NULL_MESSAGE(p, e->texto);
    return (uint32_t)strtoul(p + strlen(chave), NULL, 10);
}

static int contar(const char *tipo)
{
    int n = 0;
    for (int i = 0; i < qtd_escritas; i++) {
        n += eh_tipo(&escritas[i], tipo);
    }
    return n;
}

static void test_tel_a_5hz_em_movimento_e_1hz_parado(void)
{
    rodar(0, 10000, &EM_MOVIMENTO);
    TEST_ASSERT_EQUAL_INT(50, contar("tel"));

    telemetria_iniciar(BOOT);
    qtd_escritas = 0;
    rodar(0, 10000, &PARADO);
    TEST_ASSERT_EQUAL_INT(10, contar("tel"));
    TEST_ASSERT_EQUAL_INT(10, qtd_escritas);
}

static void test_amostra_nula_nao_envia_tel(void)
{
    rodar(0, 2000, NULL);
    TEST_ASSERT_EQUAL_INT(0, qtd_escritas);
}

static void test_seq_sobe_de_1_em_1_com_link_estavel(void)
{
    for (uint8_t x = 0; x < 10; x++) {
        inserir_evento_passo(x);
    }
    rodar(0, 1000, &EM_MOVIMENTO);
    for (uint8_t x = 10; x < 15; x++) {
        inserir_evento_passo(x);
    }
    rodar(1000, 3000, &PARADO);

    TEST_ASSERT_EQUAL_INT(15, contar("passo"));
    for (int i = 0; i < qtd_escritas; i++) {
        TEST_ASSERT_EQUAL_UINT32((uint32_t)i, campo(&escritas[i], "seq"));
        TEST_ASSERT_EQUAL_UINT32(BOOT, campo(&escritas[i], "boot"));
    }
}

static void test_eventos_com_link_caido_saem_depois_da_reconexao(void)
{
    hal_bt_sim_definir_conectado(false);
    for (uint8_t x = 0; x < 30; x++) {
        inserir_evento_passo(x);
        rodar(100u * x, 100u * x + 100, &EM_MOVIMENTO);
    }
    TEST_ASSERT_EQUAL_INT(0, qtd_escritas);

    hal_bt_sim_definir_conectado(true);
    rodar(3000, 6000, &EM_MOVIMENTO);

    TEST_ASSERT_EQUAL_INT(30, contar("passo"));
    const escrita_t *anterior = NULL;
    int tel_no_meio = 0;
    uint8_t x = 0;
    for (int i = 0; i < qtd_escritas; i++) {
        const escrita_t *e = &escritas[i];
        TEST_ASSERT_EQUAL_UINT32((uint32_t)i, campo(e, "seq")); /* a queda não consumiu seq */
        if (!eh_tipo(e, "passo")) {
            tel_no_meio += anterior != NULL && x < 30;
            continue;
        }
        TEST_ASSERT_EQUAL_UINT32(x++, campo(e, "x"));
        if (anterior != NULL) {
            TEST_ASSERT_GREATER_OR_EQUAL_UINT32(TELEMETRIA_PERIODO_EVENTO_MS, e->t - anterior->t);
        }
        anterior = e;
    }
    TEST_ASSERT_GREATER_THAN_INT(0, tel_no_meio);
}

static void test_evento_enviado_antes_da_queda_sai_com_o_mesmo_carimbo(void)
{
    inserir_evento_passo(1);
    inserir_evento_passo(2);
    rodar(0, 100, NULL);
    TEST_ASSERT_EQUAL_INT(2, qtd_escritas);
    escrita_t antes[2] = {escritas[0], escritas[1]};

    hal_bt_sim_definir_conectado(false);
    rodar(100, 500, NULL);
    hal_bt_sim_definir_conectado(true);
    rodar(500, 700, NULL);

    TEST_ASSERT_EQUAL_INT(4, qtd_escritas);
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT_EQUAL_size_t(antes[i].tamanho, escritas[2 + i].tamanho);
        TEST_ASSERT_EQUAL_MEMORY(antes[i].texto, escritas[2 + i].texto, antes[i].tamanho);
    }
}

static void test_escrita_parcial_forca_o_reenvio_desde_o_mais_antigo(void)
{
    inserir_evento_passo(1);
    inserir_evento_passo(2);
    relogio = 0;
    telemetria_passo(0, NULL); /* o 1 sai inteiro, seq 0 */

    hal_bt_sim_limitar_escrita(10);
    relogio = 50;
    telemetria_passo(50, NULL); /* o 2 sai pela metade, seq 1 */
    TEST_ASSERT_EQUAL_INT(2, qtd_escritas);
    TEST_ASSERT_EQUAL_size_t(10, escritas[1].tamanho);

    hal_bt_sim_limitar_escrita(SIZE_MAX);
    rodar(100, 300, NULL);

    TEST_ASSERT_EQUAL_INT(5, qtd_escritas);
    TEST_ASSERT_EQUAL_STRING("\n", escritas[2].texto);
    TEST_ASSERT_EQUAL_STRING(escritas[0].texto, escritas[3].texto);
    TEST_ASSERT_EQUAL_UINT32(2, campo(&escritas[4], "x"));
    TEST_ASSERT_EQUAL_UINT32(1, campo(&escritas[4], "seq"));
    TEST_ASSERT_EQUAL_UINT32(50, campo(&escritas[4], "t_ms"));
}

static void test_tel_parcial_nao_consome_seq(void)
{
    hal_bt_sim_limitar_escrita(10);
    telemetria_passo(0, &PARADO);
    hal_bt_sim_limitar_escrita(SIZE_MAX);
    relogio = 50;
    telemetria_passo(50, &PARADO);

    TEST_ASSERT_EQUAL_INT(3, qtd_escritas);
    TEST_ASSERT_EQUAL_UINT32(0, campo(&escritas[2], "seq"));
}

/* Junta o que saiu no fio e devolve a linha de número pedido (a partir de 0). */
static const char *linha_do_fio(int numero, char *destino, size_t tamanho)
{
    static char fio[MAX_ESCRITAS * 8];
    size_t usado = 0;
    for (int i = 0; i < qtd_escritas; i++) {
        TEST_ASSERT_LESS_THAN_size_t(sizeof fio, usado + escritas[i].tamanho);
        memcpy(fio + usado, escritas[i].texto, escritas[i].tamanho);
        usado += escritas[i].tamanho;
    }
    fio[usado] = '\0';
    const char *inicio = fio;
    for (int i = 0; i < numero; i++) {
        inicio = strchr(inicio, '\n');
        TEST_ASSERT_NOT_NULL(inicio);
        inicio++;
    }
    const char *fim = strchr(inicio, '\n');
    TEST_ASSERT_NOT_NULL(fim);
    size_t n = (size_t)(fim - inicio + 1);
    TEST_ASSERT_LESS_THAN_size_t(tamanho, n);
    memcpy(destino, inicio, n);
    destino[n] = '\0';
    return destino;
}

static void test_escrita_parcial_e_fechada_antes_da_proxima_linha(void)
{
    inserir_evento_passo(1);
    hal_bt_sim_limitar_escrita(10);
    telemetria_passo(0, NULL); /* o 1 sai pela metade, seq 0 */
    hal_bt_sim_limitar_escrita(SIZE_MAX);
    rodar(50, 300, NULL);

    char inteira[CONTRATO_TAMANHO_MAXIMO + 1];
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    inserir_passo(1);
    enviar(inteira, 0);

    TEST_ASSERT_EQUAL_STRING(inteira, linha_do_fio(1, linha, sizeof linha));
}

/* ---------- comando interromper ---------- */

#define INTERROMPER "{\"v\":1,\"cmd\":\"interromper\"}"

static void test_comando_interromper_liga_a_flag(void)
{
    hal_bt_sim_injetar(INTERROMPER "\n");
    telemetria_passo(0, NULL);
    TEST_ASSERT_TRUE(telemetria_consumir_interrupcao());
}

static void test_comando_partido_em_duas_voltas_liga_a_flag(void)
{
    hal_bt_sim_injetar("{\"v\":1,\"cmd\":");
    telemetria_passo(0, NULL);
    TEST_ASSERT_FALSE(telemetria_consumir_interrupcao());
    hal_bt_sim_injetar("\"interromper\"}\n");
    telemetria_passo(50, NULL);
    TEST_ASSERT_TRUE(telemetria_consumir_interrupcao());
}

static void test_comando_com_crlf_liga_a_flag(void)
{
    hal_bt_sim_injetar(INTERROMPER "\r\n");
    telemetria_passo(0, NULL);
    TEST_ASSERT_TRUE(telemetria_consumir_interrupcao());
}

static void test_lixo_nao_liga_a_flag(void)
{
    hal_bt_sim_injetar("lixo\n{\"v\":2,\"cmd\":\"interromper\"}\n\n");
    telemetria_passo(0, NULL);
    TEST_ASSERT_FALSE(telemetria_consumir_interrupcao());
}

static void test_linha_longa_e_descartada_e_a_seguinte_e_lida(void)
{
    /* 300 bytes antes do "\n", com o comando no fim: a linha passa de 256. */
    char pedaco[101];
    memset(pedaco, 'x', 100);
    pedaco[100] = '\0';
    hal_bt_sim_injetar(pedaco);
    telemetria_passo(0, NULL);
    hal_bt_sim_injetar(pedaco);
    telemetria_passo(50, NULL);
    hal_bt_sim_injetar(pedaco + 30);
    telemetria_passo(100, NULL);
    hal_bt_sim_injetar(INTERROMPER "\n");
    telemetria_passo(150, NULL);
    TEST_ASSERT_FALSE(telemetria_consumir_interrupcao());

    hal_bt_sim_injetar(INTERROMPER "\n");
    telemetria_passo(200, NULL);
    TEST_ASSERT_TRUE(telemetria_consumir_interrupcao());
}

static void test_consumir_duas_vezes_devolve_true_e_depois_false(void)
{
    hal_bt_sim_injetar(INTERROMPER "\n");
    telemetria_passo(0, NULL);
    TEST_ASSERT_TRUE(telemetria_consumir_interrupcao());
    TEST_ASSERT_FALSE(telemetria_consumir_interrupcao());
}

static void test_boot_sobe_a_cada_chamada_e_volta_a_1_sem_a_nvs(void)
{
    hal_nvs_sim_apagar_tudo();
    TEST_ASSERT_TRUE(hal_nvs_iniciar());
    TEST_ASSERT_EQUAL_UINT32(1, telemetria_proximo_boot());
    TEST_ASSERT_EQUAL_UINT32(2, telemetria_proximo_boot());
    TEST_ASSERT_EQUAL_UINT32(3, telemetria_proximo_boot());

    hal_nvs_sim_apagar_tudo();
    TEST_ASSERT_EQUAL_UINT32(1, telemetria_proximo_boot());
}

static void test_boot_com_gravacao_falhando_devolve_o_valor_somado(void)
{
    /* Enche a NVS simulada com outras chaves: a gravação do boot falha. */
    hal_nvs_sim_apagar_tudo();
    char chave[8];
    uint8_t byte = 0;
    for (int i = 0;; i++) {
        snprintf(chave, sizeof(chave), "k%d", i);
        if (!hal_nvs_gravar(chave, &byte, sizeof(byte))) {
            break;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(1, telemetria_proximo_boot());
    TEST_ASSERT_EQUAL_UINT32(1, telemetria_proximo_boot());
    hal_nvs_sim_apagar_tudo();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ordem_de_saida_igual_a_de_insercao);
    RUN_TEST(test_70_insercoes_guardam_as_64_mais_novas);
    RUN_TEST(test_voltar_o_cursor_reenvia_as_mesmas_linhas);
    RUN_TEST(test_descartado_sem_sair_nao_consome_seq);
    RUN_TEST(test_carimbado_descartado_leva_carimbados_ate_junto);
    RUN_TEST(test_cada_tipo_usa_o_serializador_do_contrato);
    RUN_TEST(test_sem_evento_pendente_nao_serializa);
    RUN_TEST(test_tel_a_5hz_em_movimento_e_1hz_parado);
    RUN_TEST(test_amostra_nula_nao_envia_tel);
    RUN_TEST(test_seq_sobe_de_1_em_1_com_link_estavel);
    RUN_TEST(test_eventos_com_link_caido_saem_depois_da_reconexao);
    RUN_TEST(test_evento_enviado_antes_da_queda_sai_com_o_mesmo_carimbo);
    RUN_TEST(test_escrita_parcial_forca_o_reenvio_desde_o_mais_antigo);
    RUN_TEST(test_tel_parcial_nao_consome_seq);
    RUN_TEST(test_escrita_parcial_e_fechada_antes_da_proxima_linha);
    RUN_TEST(test_comando_interromper_liga_a_flag);
    RUN_TEST(test_comando_partido_em_duas_voltas_liga_a_flag);
    RUN_TEST(test_comando_com_crlf_liga_a_flag);
    RUN_TEST(test_lixo_nao_liga_a_flag);
    RUN_TEST(test_linha_longa_e_descartada_e_a_seguinte_e_lida);
    RUN_TEST(test_consumir_duas_vezes_devolve_true_e_depois_false);
    RUN_TEST(test_boot_sobe_a_cada_chamada_e_volta_a_1_sem_a_nvs);
    RUN_TEST(test_boot_com_gravacao_falhando_devolve_o_valor_somado);
    return UNITY_END();
}
