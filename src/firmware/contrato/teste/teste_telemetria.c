/*
 * Confere que o serializador gera, byte a byte, cada linha de
 * src/contrato/exemplos.jsonl. Roda no PC: make -C src/firmware/contrato teste
 */
#include <stdio.h>
#include <string.h>

#include "../telemetria.h"

#define MAX_LINHAS 32

static int falhas = 0;

static void conferir(int indice, const char *gerada, int n, const char *esperada)
{
    if (n < 0 || strcmp(gerada, esperada) != 0) {
        falhas++;
        printf("FALHOU linha %d\n  esperada: %s  gerada:   %s\n", indice + 1, esperada,
               n < 0 ? "(não coube)\n" : gerada);
    } else {
        printf("ok       linha %d\n", indice + 1);
    }
}

int main(int argc, char **argv)
{
    const char *caminho = argc > 1 ? argv[1] : "../../contrato/exemplos.jsonl";
    FILE *arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        perror(caminho);
        return 2;
    }
    static char esperadas[MAX_LINHAS][CONTRATO_TAMANHO_MAXIMO + 2];
    int total = 0;
    while (total < MAX_LINHAS && fgets(esperadas[total], sizeof esperadas[total], arquivo)) {
        total++;
    }
    fclose(arquivo);

    char buf[CONTRATO_TAMANHO_MAXIMO + 1];
    int i = 0;

    /* Mesma ordem de exemplos.jsonl. */
    {
        contrato_cabecalho_t c = {7, 0, 412};
        contrato_hc_item_t m = {CONTRATO_COMPONENTE_BATERIA, true, true, 7840};
        conferir(i, buf, contrato_hc_item(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {7, 5, 1630};
        contrato_hc_item_t m = {CONTRATO_COMPONENTE_MOTOR_ESQUERDO, true, false, 0};
        conferir(i, buf, contrato_hc_item(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {7, 10, 2380};
        contrato_hc_resultado_t m = {true, CONTRATO_DIP_4X4, CONTRATO_INICIO_NOVA};
        conferir(i, buf, contrato_hc_resultado(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {7, 42, 11250};
        contrato_tel_t m = {CONTRATO_ESTADO_RUNNING, 0, 2, CONTRATO_RUMO_N, 7810, 210,
                            CONTRATO_EIXO_DESCONHECIDO};
        conferir(i, buf, contrato_tel(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {7, 43, 11400};
        contrato_passo_t m = {0, 3, CONTRATO_PAREDE_N | CONTRATO_PAREDE_O};
        conferir(i, buf, contrato_passo(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {7, 88, 25120};
        contrato_falha_t m = {CONTRATO_MOTIVO_FALHA_COMPONENTE, CONTRATO_ORIGEM_AUTOMATICA, 2, 3,
                              CONTRATO_COMPONENTE_TOF_DIREITO};
        conferir(i, buf, contrato_falha(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {8, 57, 19870};
        contrato_falha_t m = {CONTRATO_MOTIVO_NENHUM, CONTRATO_ORIGEM_WEB, 1, 2,
                              CONTRATO_COMPONENTE_NENHUM};
        conferir(i, buf, contrato_falha(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {9, 63, 21040};
        contrato_falha_t m = {CONTRATO_MOTIVO_ENCERRADO_OPERADOR, CONTRATO_ORIGEM_BOOT, 1, 3,
                              CONTRATO_COMPONENTE_NENHUM};
        conferir(i, buf, contrato_falha(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }
    {
        contrato_cabecalho_t c = {11, 131, 48200};
        contrato_sucesso_t m = {3, 3};
        conferir(i, buf, contrato_sucesso(buf, sizeof buf, &c, &m), esperadas[i]);
        i++;
    }

    if (i != total) {
        falhas++;
        printf("FALHOU: o teste gera %d linhas e exemplos.jsonl tem %d\n", i, total);
    }

    /* Pior caso: maiores valores e textos mais longos ainda cabem em 256 bytes. */
    {
        contrato_cabecalho_t c = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
        contrato_tel_t t = {CONTRATO_ESTADO_HEALTH_CHECK, 11, 11, CONTRATO_RUMO_O, UINT16_MAX,
                            UINT16_MAX, CONTRATO_EIXO_X};
        contrato_falha_t f = {CONTRATO_MOTIVO_FALHA_COMPONENTE, CONTRATO_ORIGEM_AUTOMATICA, 11, 11,
                              CONTRATO_COMPONENTE_ENCODER_ESQUERDO};
        contrato_hc_item_t h = {CONTRATO_COMPONENTE_TOF_FRONTAL_ESQ, false, true, INT32_MIN};
        if (contrato_tel(buf, sizeof buf, &c, &t) < 0 || contrato_falha(buf, sizeof buf, &c, &f) < 0 ||
            contrato_hc_item(buf, sizeof buf, &c, &h) < 0) {
            falhas++;
            printf("FALHOU: mensagem de pior caso passou de %d bytes\n", CONTRATO_TAMANHO_MAXIMO);
        } else {
            printf("ok       pior caso cabe em %d bytes\n", CONTRATO_TAMANHO_MAXIMO);
        }
    }

    /* Buffer pequeno demais devolve -1 em vez de cortar a linha. */
    {
        char pequeno[32];
        contrato_cabecalho_t c = {7, 1, 1};
        contrato_sucesso_t s = {3, 3};
        if (contrato_sucesso(pequeno, sizeof pequeno, &c, &s) != -1) {
            falhas++;
            printf("FALHOU: buffer pequeno não devolveu -1\n");
        } else {
            printf("ok       buffer pequeno devolve -1\n");
        }
    }

    /* Comando da web. */
    if (!contrato_eh_interromper("{\"v\":1,\"cmd\":\"interromper\"}\n") ||
        contrato_eh_interromper("{\"v\":2,\"cmd\":\"interromper\"}\n") ||
        contrato_eh_interromper("{\"v\":1,\"cmd\":\"andar\"}\n")) {
        falhas++;
        printf("FALHOU: reconhecimento do comando interromper\n");
    } else {
        printf("ok       comando interromper\n");
    }

    printf("%s (%d falha(s))\n", falhas ? "FALHOU" : "OK", falhas);
    return falhas ? 1 : 0;
}
