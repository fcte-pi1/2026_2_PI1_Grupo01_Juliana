/*
 * Telemetria — FIRM-07 (#123). Buffer de 64 eventos com cursor de envio e
 * carimbo no primeiro envio.
 */

#include <string.h>
#include <unity.h>

/* Caminho relativo: lib/contrato também tem um telemetria.h. */
#include "../../lib/telemetria/telemetria.h"

#define BOOT 7

static telemetria_buffer_t buffer;
static uint32_t seq;

void setUp(void)
{
    telemetria_buffer_iniciar(&buffer);
    seq = 0;
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
    return UNITY_END();
}
