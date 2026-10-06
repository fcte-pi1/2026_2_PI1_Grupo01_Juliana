#include "sim_roteiro.h"

#include <stdarg.h>
#include <stdio.h>

#include "config/robo.h"

/* Modelo de tempo: velocidade constante e giros com duração fixa. */
#define VELOCIDADE_MM_S      200
#define CELULA_MS            (CELULA_MM * 1000 / VELOCIDADE_MM_S)
#define GIRO_90_MS           500
#define GIRO_180_MS          1000
#define PERIODO_TEL_MOVENDO  200   /* 5 Hz */
#define PERIODO_TEL_PARADO   1000  /* 1 Hz */
#define PERIODO_HC_ITEM      250

/* Valores fictícios do health-check (tudo aprovado). */
#define HC_BATERIA_MV        8100
#define HC_TOF_FRENTE_MM     300
#define HC_TOF_LADO_MM       54
#define HC_ENCODER_PULSOS    ENCODER_PULSOS_POR_VOLTA
#define DESCARGA_MS_POR_MV   500   /* a tensão cai 2 mV por segundo */

static const char RUMO[4] = {'N', 'L', 'S', 'O'};

/* Escreve o cabeçalho comum e o resto da mensagem, já com o fecha-chaves e o "\n". */
static void emitir(sim_roteiro_t *r, const char *tipo, const char *formato, ...)
{
    char linha[SIM_ROTEIRO_TAMANHO_LINHA + 1];
    int n = snprintf(linha, sizeof(linha), "{\"v\":1,\"boot\":%lu,\"seq\":%lu,\"t_ms\":%lu,\"tipo\":\"%s\",",
                     (unsigned long)r->boot, (unsigned long)r->seq, (unsigned long)r->t_ms, tipo);
    va_list args;
    va_start(args, formato);
    n += vsnprintf(linha + n, sizeof(linha) - (size_t)n, formato, args);
    va_end(args);

    if (n < SIM_ROTEIRO_TAMANHO_LINHA) { /* o contrato limita a 256 bytes com o "\n" */
        r->seq++;
        r->saida(r->saida_ctx, linha);
    }
}

static void emitir_tel(sim_roteiro_t *r, int vel_mm_s)
{
    char eixo[8] = "null";
    if (r->eixo_longo) {
        snprintf(eixo, sizeof(eixo), "\"%c\"", r->eixo_longo);
    }
    uint32_t queda = r->t_ms / DESCARGA_MS_POR_MV;
    emitir(r, "tel", "\"estado\":\"%s\",\"x\":%u,\"y\":%u,\"rumo\":\"%c\",\"bat_mv\":%lu,\"vel_mm_s\":%d,\"eixo_longo\":%s}\n",
           r->estado, r->x, r->y, RUMO[r->rumo], (unsigned long)(HC_BATERIA_MV - queda), vel_mm_s, eixo);
}

/* Passa o tempo, enviando as "tel" que caírem dentro do intervalo. */
static void avancar(sim_roteiro_t *r, uint32_t duracao_ms, bool movendo, int vel_mm_s)
{
    uint32_t fim = r->t_ms + duracao_ms;
    while (r->proxima_tel_ms <= fim) {
        r->t_ms = r->proxima_tel_ms;
        emitir_tel(r, vel_mm_s);
        r->proxima_tel_ms += movendo ? PERIODO_TEL_MOVENDO : PERIODO_TEL_PARADO;
    }
    r->t_ms = fim;
}

static void emitir_hc_item(sim_roteiro_t *r, const char *componente, long valor, bool tem_valor)
{
    avancar(r, PERIODO_HC_ITEM, false, 0);
    if (tem_valor) {
        emitir(r, "hc_item", "\"componente\":\"%s\",\"aprovado\":true,\"valor\":%ld}\n", componente, valor);
    } else {
        emitir(r, "hc_item", "\"componente\":\"%s\",\"aprovado\":true,\"valor\":null}\n", componente);
    }
}

void sim_roteiro_iniciar(sim_roteiro_t *r, uint32_t boot, sim_tipo_t tipo, sim_saida_t saida, void *saida_ctx)
{
    r->saida = saida;
    r->saida_ctx = saida_ctx;
    r->boot = boot;
    r->seq = 0;
    r->t_ms = 0;
    r->proxima_tel_ms = PERIODO_TEL_PARADO;
    r->estado = "health-check";
    r->x = 0;
    r->y = 0;
    r->rumo = NAV_NORTE;
    r->eixo_longo = 0;

    emitir_hc_item(r, "bateria", HC_BATERIA_MV, true);
    emitir_hc_item(r, "tof_frontal_esq", HC_TOF_FRENTE_MM, true);
    emitir_hc_item(r, "tof_frontal_dir", HC_TOF_FRENTE_MM, true);
    emitir_hc_item(r, "tof_esquerdo", HC_TOF_LADO_MM, true);
    emitir_hc_item(r, "tof_direito", HC_TOF_LADO_MM, true);
    emitir_hc_item(r, "motor_esquerdo", 0, false);
    emitir_hc_item(r, "encoder_esquerdo", HC_ENCODER_PULSOS, true);
    emitir_hc_item(r, "motor_direito", 0, false);
    emitir_hc_item(r, "encoder_direito", HC_ENCODER_PULSOS, true);

    avancar(r, PERIODO_HC_ITEM, false, 0);
    emitir(r, "hc_resultado", "\"aprovado\":true,\"tipo_dip\":\"%s\",\"inicio\":\"nova\"}\n", sim_tipo_nome(tipo));

    r->estado = "running";
    r->proxima_tel_ms = r->t_ms + PERIODO_TEL_MOVENDO;
}

void sim_roteiro_celula(void *roteiro, const sim_celula_t *c)
{
    sim_roteiro_t *r = (sim_roteiro_t *)roteiro;

    r->x = c->x;
    r->y = c->y;
    r->rumo = c->rumo;
    /* O robô descobre o lado longo ao passar da 4ª célula em x ou em y (docs/4.4.1). */
    if (!r->eixo_longo && (c->x > 3 || c->y > 3)) {
        r->eixo_longo = c->x > 3 ? 'x' : 'y';
    }
    emitir(r, "passo", "\"x\":%u,\"y\":%u,\"paredes\":%u}\n", c->x, c->y, c->paredes);

    uint32_t giro_ms = 0;
    switch (c->acao) {
    case NAV_ACAO_FRENTE:
        break;
    case NAV_ACAO_DIREITA:
    case NAV_ACAO_ESQUERDA:
        giro_ms = GIRO_90_MS;
        break;
    case NAV_ACAO_MEIA_VOLTA:
        giro_ms = GIRO_180_MS;
        break;
    default:
        return; /* sucesso ou sem passo: o robô para aqui */
    }
    avancar(r, giro_ms, true, 0);
    r->rumo = c->rumo_depois;
    avancar(r, CELULA_MS, true, VELOCIDADE_MM_S);
}

void sim_roteiro_finalizar(sim_roteiro_t *r, const sim_relatorio_t *rel)
{
    switch (rel->resultado) {
    case SIM_RESULTADO_SUCESSO:
    case SIM_RESULTADO_SUCESSO_FALSO: /* o robô acredita que chegou e avisa */
        r->estado = "success";
        emitir(r, "sucesso", "\"x\":%u,\"y\":%u}\n", rel->x, rel->y);
        break;
    default:
        r->estado = "failed";
        emitir(r, "falha", "\"motivo\":\"%s\",\"origem\":\"automatica\",\"x\":%u,\"y\":%u,\"componente\":null}\n",
               rel->resultado == SIM_RESULTADO_COLISAO ? "collision" : "stuck", rel->x, rel->y);
        break;
    }
    emitir_tel(r, 0);
}
