/*
 * Simulador de navegação no PC (FIRM-02, #114).
 *
 * Roda a navegação do firmware (lib/navegacao, o mesmo código da ESP32) em
 * labirintos desenhados em texto e escreve uma linha de relatório por
 * labirinto. Uso e formato da saída: simulador/README.md.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "sim_corrida.h"
#include "sim_labirinto.h"
#include "sim_roteiro.h"

#define TAMANHO_ARQUIVO 8192

typedef struct {
    int ruido_pct;
    unsigned long semente;
    unsigned long boot;
    unsigned long limite;
    const char *pasta_telemetria;
    int exigir_sucesso;
} opcoes_t;

static void uso(FILE *saida)
{
    fprintf(saida,
            "uso: simulador [opcoes] labirinto.txt...\n"
            "\n"
            "  --ruido PCT        chance (0 a 100) de cada leitura de parede vir trocada (padrao 0)\n"
            "  --semente N        semente do sorteio do ruido (padrao 1)\n"
            "  --limite N         passos antes de desistir (padrao %d)\n"
            "  --telemetria PASTA grava PASTA/<labirinto>.jsonl no formato do contrato v1 (ARQ-01)\n"
            "  --boot N           campo \"boot\" do roteiro de telemetria (padrao 1)\n"
            "  --exigir-sucesso   sai com codigo 1 se algum labirinto nao terminar em sucesso\n",
            SIM_LIMITE_PASSOS_PADRAO);
}

static int ler_numero(const char *texto, unsigned long maximo, unsigned long *valor)
{
    char *fim;
    errno = 0;
    unsigned long v = strtoul(texto, &fim, 10);
    if (errno != 0 || *texto == '\0' || *fim != '\0' || v > maximo) {
        return 0;
    }
    *valor = v;
    return 1;
}

static int ler_arquivo(const char *caminho, char *texto, size_t tamanho)
{
    FILE *f = fopen(caminho, "r");
    if (f == NULL) {
        return 0;
    }
    size_t n = fread(texto, 1, tamanho - 1, f);
    int completo = feof(f);
    fclose(f);
    texto[n] = '\0';
    return completo;
}

/* "labirintos/8x4-frente-01-esq.txt" -> "8x4-frente-01-esq" */
static void nome_do_labirinto(const char *caminho, char *nome, size_t tamanho)
{
    const char *barra = strrchr(caminho, '/');
    snprintf(nome, tamanho, "%s", barra ? barra + 1 : caminho);
    char *ponto = strrchr(nome, '.');
    if (ponto != NULL && ponto != nome) {
        *ponto = '\0';
    }
}

static void escrever_no_arquivo(void *ctx, const char *linha) { fputs(linha, (FILE *)ctx); }

/* Devolve 0 = sucesso, 1 = a navegação falhou, 2 = labirinto ou arquivo com problema. */
static int simular(const char *caminho, const opcoes_t *op)
{
    static char texto[TAMANHO_ARQUIVO];
    char nome[128];
    char erro[96];
    sim_labirinto_t lab;

    nome_do_labirinto(caminho, nome, sizeof(nome));

    if (!ler_arquivo(caminho, texto, sizeof(texto))) {
        printf("%-24s INVALIDO  nao foi possivel ler o arquivo\n", nome);
        return 2;
    }
    if (!sim_labirinto_carregar(&lab, texto)) {
        printf("%-24s INVALIDO  desenho malformado\n", nome);
        return 2;
    }
    if (!sim_validar(&lab, erro, sizeof(erro))) {
        printf("%-24s INVALIDO  %s\n", nome, erro);
        return 2;
    }

    sim_config_t config = {0};
    config.ruido_pct = (uint8_t)op->ruido_pct;
    config.semente = (uint32_t)op->semente;
    config.limite_passos = (uint16_t)op->limite;

    FILE *telemetria = NULL;
    sim_roteiro_t roteiro;
    if (op->pasta_telemetria != NULL) {
        char arquivo[512];
        snprintf(arquivo, sizeof(arquivo), "%s/%s.jsonl", op->pasta_telemetria, nome);
        telemetria = fopen(arquivo, "w");
        if (telemetria == NULL) {
            printf("%-24s ERRO      nao foi possivel criar %s\n", nome, arquivo);
            return 2;
        }
        sim_roteiro_iniciar(&roteiro, (uint32_t)op->boot, sim_tipo(&lab), escrever_no_arquivo, telemetria);
        config.observador = sim_roteiro_celula;
        config.observador_ctx = &roteiro;
    }

    sim_relatorio_t rel;
    sim_correr(&lab, &config, &rel);

    if (telemetria != NULL) {
        sim_roteiro_finalizar(&roteiro, &rel);
        fclose(telemetria);
    }

    printf("%-24s %-9s %-13s celulas=%-4u distintas=%-3u giros90=%-4u giros180=%-3u leituras_erradas=%u\n",
           nome, sim_tipo_nome(rel.tipo), sim_resultado_nome(rel.resultado), rel.celulas, rel.distintas,
           rel.giros_90, rel.giros_180, rel.leituras_erradas);
    return rel.resultado == SIM_RESULTADO_SUCESSO ? 0 : 1;
}

int main(int argc, char **argv)
{
    opcoes_t op = {0, 1, 1, 0, NULL, 0};
    int primeiro = 1;

    for (; primeiro < argc && strncmp(argv[primeiro], "--", 2) == 0; primeiro++) {
        const char *opcao = argv[primeiro];
        const char *valor = primeiro + 1 < argc ? argv[primeiro + 1] : NULL;
        unsigned long n;

        if (strcmp(opcao, "--exigir-sucesso") == 0) {
            op.exigir_sucesso = 1;
            continue;
        }
        if (strcmp(opcao, "--ajuda") == 0 || strcmp(opcao, "--help") == 0) {
            uso(stdout);
            return 0;
        }
        if (valor == NULL) {
            fprintf(stderr, "simulador: %s precisa de um valor\n", opcao);
            return 2;
        }
        primeiro++;
        if (strcmp(opcao, "--ruido") == 0 && ler_numero(valor, 100, &n)) {
            op.ruido_pct = (int)n;
        } else if (strcmp(opcao, "--semente") == 0 && ler_numero(valor, UINT32_MAX, &n)) {
            op.semente = n;
        } else if (strcmp(opcao, "--limite") == 0 && ler_numero(valor, UINT16_MAX, &n)) {
            op.limite = n;
        } else if (strcmp(opcao, "--boot") == 0 && ler_numero(valor, UINT32_MAX, &n)) {
            op.boot = n;
        } else if (strcmp(opcao, "--telemetria") == 0) {
            op.pasta_telemetria = valor;
        } else {
            fprintf(stderr, "simulador: opcao ou valor invalido: %s %s\n", opcao, valor);
            uso(stderr);
            return 2;
        }
    }

    if (primeiro == argc) {
        uso(stderr);
        return 2;
    }
    if (op.pasta_telemetria != NULL && mkdir(op.pasta_telemetria, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "simulador: nao foi possivel criar a pasta %s\n", op.pasta_telemetria);
        return 2;
    }

    int total = 0, sucessos = 0, invalidos = 0;
    for (int i = primeiro; i < argc; i++) {
        int r = simular(argv[i], &op);
        total++;
        sucessos += r == 0;
        invalidos += r == 2;
    }
    printf("total: %d labirintos, %d sucesso, %d falha, %d invalido\n", total, sucessos,
           total - sucessos - invalidos, invalidos);

    if (invalidos > 0) {
        return 2;
    }
    return op.exigir_sucesso && sucessos < total ? 1 : 0;
}
