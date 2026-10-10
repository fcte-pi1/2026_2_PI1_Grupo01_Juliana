#include "telemetria.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *const ESTADOS[] = {"health-check", "running", "success", "failed"};
static const char *const RUMOS[] = {"N", "S", "L", "O"};
static const char *const EIXOS[] = {"null", "\"x\"", "\"y\""};
static const char *const COMPONENTES[] = {
    "null",
    "\"bateria\"",
    "\"tof_frontal\"",
    "\"tof_esquerdo\"",
    "\"tof_direito\"",
    "\"motor_esquerdo\"",
    "\"motor_direito\"",
    "\"encoder_esquerdo\"",
    "\"encoder_direito\"",
};
static const char *const TIPOS_DIP[] = {"4x4", "8x4", "12x4", "invalido"};
static const char *const INICIOS[] = {"nova", "retomada"};
static const char *const MOTIVOS[] = {
    "null",
    "\"collision\"",
    "\"stuck\"",
    "\"falha_componente\"",
    "\"low_battery\"",
    "\"encerrado_operador\"",
};
static const char *const ORIGENS[] = {"automatica", "web", "boot"};

#define BOOL(b) ((b) ? "true" : "false")

/* Escreve o cabeçalho comum, o corpo da mensagem e "}\n". */
static int escrever(char *buf, size_t tam, const contrato_cabecalho_t *cab, const char *tipo,
                    const char *formato, ...)
{
    size_t limite = tam < CONTRATO_TAMANHO_MAXIMO + 1 ? tam : CONTRATO_TAMANHO_MAXIMO + 1;
    int n = snprintf(buf, limite,
                     "{\"v\":%d,\"boot\":%" PRIu32 ",\"seq\":%" PRIu32 ",\"t_ms\":%" PRIu32
                     ",\"tipo\":\"%s\"",
                     CONTRATO_VERSAO, cab->boot, cab->seq, cab->t_ms, tipo);
    if (n < 0 || (size_t)n >= limite) {
        return -1;
    }

    va_list args;
    va_start(args, formato);
    int corpo = vsnprintf(buf + n, limite - (size_t)n, formato, args);
    va_end(args);
    if (corpo < 0 || (size_t)(n + corpo) >= limite) {
        return -1;
    }
    n += corpo;

    int fim = snprintf(buf + n, limite - (size_t)n, "}\n");
    if (fim < 0 || (size_t)(n + fim) >= limite) {
        return -1;
    }
    return n + fim;
}

int contrato_tel(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_tel_t *m)
{
    return escrever(buf, tam, cab, "tel",
                    ",\"estado\":\"%s\",\"x\":%u,\"y\":%u,\"rumo\":\"%s\",\"bat_mv\":%u,"
                    "\"vel_mm_s\":%u,\"eixo_longo\":%s",
                    ESTADOS[m->estado], (unsigned)m->x, (unsigned)m->y, RUMOS[m->rumo],
                    (unsigned)m->bat_mv, (unsigned)m->vel_mm_s, EIXOS[m->eixo_longo]);
}

int contrato_hc_item(char *buf, size_t tam, const contrato_cabecalho_t *cab,
                     const contrato_hc_item_t *m)
{
    if (m->tem_valor) {
        return escrever(buf, tam, cab, "hc_item",
                        ",\"componente\":%s,\"aprovado\":%s,\"valor\":%" PRId32,
                        COMPONENTES[m->componente], BOOL(m->aprovado), m->valor);
    }
    return escrever(buf, tam, cab, "hc_item", ",\"componente\":%s,\"aprovado\":%s,\"valor\":null",
                    COMPONENTES[m->componente], BOOL(m->aprovado));
}

int contrato_hc_resultado(char *buf, size_t tam, const contrato_cabecalho_t *cab,
                          const contrato_hc_resultado_t *m)
{
    return escrever(buf, tam, cab, "hc_resultado",
                    ",\"aprovado\":%s,\"tipo_dip\":\"%s\",\"inicio\":\"%s\"", BOOL(m->aprovado),
                    TIPOS_DIP[m->tipo_dip], INICIOS[m->inicio]);
}

int contrato_passo(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_passo_t *m)
{
    return escrever(buf, tam, cab, "passo", ",\"x\":%u,\"y\":%u,\"paredes\":%u", (unsigned)m->x,
                    (unsigned)m->y, (unsigned)m->paredes);
}

int contrato_falha(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_falha_t *m)
{
    return escrever(buf, tam, cab, "falha",
                    ",\"motivo\":%s,\"origem\":\"%s\",\"x\":%u,\"y\":%u,\"componente\":%s",
                    MOTIVOS[m->motivo], ORIGENS[m->origem], (unsigned)m->x, (unsigned)m->y,
                    COMPONENTES[m->componente]);
}

int contrato_sucesso(char *buf, size_t tam, const contrato_cabecalho_t *cab,
                     const contrato_sucesso_t *m)
{
    return escrever(buf, tam, cab, "sucesso", ",\"x\":%u,\"y\":%u", (unsigned)m->x, (unsigned)m->y);
}

bool contrato_eh_interromper(const char *linha)
{
    return strstr(linha, "\"v\":1") != NULL && strstr(linha, "\"cmd\":\"interromper\"") != NULL;
}
