#include "movimento.h"

#include <math.h>
#include <stdlib.h>

#include "config/robo.h"
#include "hal_encoder.h"
#include "hal_motor.h"

#define DT_S         (MOV_PERIODO_MS / 1000.0f)

typedef enum { TIPO_AVANCO, TIPO_GIRO } tipo_t;

/*
 * Cada roda segue uma referência de posição em pulsos, que anda pela rampa
 * (trapézio): acelera até a velocidade máxima e desacelera para parar no total.
 * No giro a roda esquerda segue a mesma referência com o sinal trocado.
 * Seguir a referência roda a roda já mantém o avanço reto; o desvio entre as
 * rodas só existe para a centralização pelos ToF.
 */
static struct {
    mov_estado_t estado;
    tipo_t tipo;
    int32_t inicio[MOTOR_QTD]; /* encoder no início do movimento */
    float total;               /* pulsos a andar por roda (sem sinal) */
    float referencia;          /* onde a roda deveria estar agora, em pulsos */
    float velocidade;          /* da referência, em mm/s */
    float aceleracao;          /* da referência, em mm/s² */
    float velocidade_max;
    float desvio;              /* pulsos que a roda direita vai à frente da esquerda */
    uint32_t decorrido_ms;
    uint32_t assentado_ms;
    int32_t ultimo[MOTOR_QTD];      /* última leitura do encoder, para ver a roda travada */
    uint32_t parado_ms[MOTOR_QTD];  /* tempo da roda sem contar pulsos com potência alta */
    mov_tof_t tof;
} mov;

int32_t mov_pulsos_avanco(float mm)
{
    return (int32_t)lroundf(mm / MM_POR_PULSO);
}

int32_t mov_pulsos_giro(int graus)
{
    /* Girando no lugar, cada roda anda o arco de raio BITOLA_MM / 2. */
    float arco_mm = PI_F * BITOLA_MM * (float)graus / 360.0f;
    return (int32_t)lroundf(arco_mm / MM_POR_PULSO);
}

static float limitar(float potencia)
{
    return fmaxf(-MOTOR_POTENCIA_MAX, fminf(MOTOR_POTENCIA_MAX, potencia));
}

static void definir_potencia(float esquerda, float direita)
{
    hal_motor_definir(MOTOR_ESQ, (int16_t)lroundf(limitar(esquerda)));
    hal_motor_definir(MOTOR_DIR, (int16_t)lroundf(limitar(direita)));
}

/* Corta a potência e põe a ponte H em standby até o próximo movimento. */
static void desligar(mov_estado_t estado)
{
    definir_potencia(0.0f, 0.0f);
    hal_motor_habilitar(false);
    mov.estado = estado;
}

/* Conta o tempo de cada roda parada com potência alta; true se alguma travou. */
static bool roda_travada(float esquerda, float direita)
{
    float potencia[MOTOR_QTD];
    potencia[MOTOR_ESQ] = limitar(esquerda);
    potencia[MOTOR_DIR] = limitar(direita);
    bool travou = false;
    for (int i = 0; i < MOTOR_QTD; i++) {
        int32_t leitura = hal_encoder_ler(i);
        if (leitura != mov.ultimo[i] || fabsf(potencia[i]) < MOV_TRAVADO_POTENCIA) {
            mov.ultimo[i] = leitura;
            mov.parado_ms[i] = 0;
        } else {
            mov.parado_ms[i] += MOV_PERIODO_MS;
        }
        travou = travou || mov.parado_ms[i] >= MOV_TRAVADO_MS;
    }
    return travou;
}

static void iniciar(tipo_t tipo, int32_t pulsos, float velocidade_max)
{
    mov.tipo = tipo;
    mov.inicio[MOTOR_ESQ] = hal_encoder_ler(MOTOR_ESQ);
    mov.inicio[MOTOR_DIR] = hal_encoder_ler(MOTOR_DIR);
    mov.total = (float)labs(pulsos);
    mov.referencia = 0.0f;
    mov.velocidade = 0.0f;
    mov.aceleracao = 0.0f;
    mov.velocidade_max = velocidade_max;
    mov.desvio = 0.0f;
    mov.decorrido_ms = 0;
    mov.assentado_ms = 0;
    for (int i = 0; i < MOTOR_QTD; i++) {
        mov.ultimo[i] = mov.inicio[i];
        mov.parado_ms[i] = 0;
    }
    mov.tof = (mov_tof_t){0};
    mov.estado = MOV_EM_CURSO;
    hal_motor_habilitar(true);
}

void mov_iniciar(void)
{
    hal_motor_iniciar();
    hal_encoder_iniciar();
    mov.estado = MOV_LIVRE;
}

bool mov_avancar_celula(void)
{
    if (mov.estado == MOV_EM_CURSO) {
        return false;
    }
    iniciar(TIPO_AVANCO, mov_pulsos_avanco(CELULA_MM), VELOCIDADE_CRUZEIRO_MM_S);
    return true;
}

bool mov_girar(int graus)
{
    if (mov.estado == MOV_EM_CURSO || (graus != 90 && graus != -90 && graus != 180)) {
        return false;
    }
    iniciar(TIPO_GIRO, mov_pulsos_giro(graus), MOV_VELOCIDADE_GIRO_MM_S);
    /* O sinal fica no total da roda direita: positivo gira para a esquerda. */
    if (graus < 0) {
        mov.total = -mov.total;
    }
    return true;
}

void mov_parar(void)
{
    desligar(MOV_LIVRE);
}

mov_estado_t mov_estado(void)
{
    return mov.estado;
}

void mov_informar_tof(const mov_tof_t *tof)
{
    if (tof != NULL) {
        mov.tof = *tof;
    }
}

/*
 * Centralização (FIRM-10): mm que o robô está à direita do centro do corredor,
 * pelos laterais. Com as duas paredes compara uma com a outra; com uma só,
 * compara com a leitura do robô centrado.
 */
static float erro_lateral_mm(void)
{
    bool tem_esquerda = mov.tof.parede_esquerda && mov.tof.esquerda_mm > 0;
    bool tem_direita = mov.tof.parede_direita && mov.tof.direita_mm > 0;
    float esquerda = (float)mov.tof.esquerda_mm;
    float direita = (float)mov.tof.direita_mm;

    if (tem_esquerda && tem_direita) {
        return (esquerda - direita) / 2.0f;
    }
    if (tem_esquerda) {
        return esquerda - MOV_LADO_CENTRADO_MM;
    }
    if (tem_direita) {
        return MOV_LADO_CENTRADO_MM - direita;
    }
    return 0.0f;
}

/* Correção frontal (FIRM-10): com parede perto, o fim do avanço passa a seguir a distância medida. */
static void corrigir_pela_frente(float andado)
{
    uint16_t frente = mov.tof.frente_mm;
    if (!mov.tof.parede_frente || frente == 0 || frente >= PAREDE_FRENTE_ENTRA_MM) {
        return;
    }
    float estimado = andado + (float)mov_pulsos_avanco((float)frente - MOV_FRENTE_ALVO_MM);
    mov.total += MOV_GANHO_FRONTAL * (estimado - mov.total);
    if (mov.total < 0.0f) {
        mov.total = 0.0f;
    }
}

/* Avança a referência pela rampa: acelera, segue na máxima e freia para parar no total. */
static void avancar_referencia(float total)
{
    float restante_mm = (total - mov.referencia) * MM_POR_PULSO;
    if (restante_mm < 0.0f) {
        restante_mm = 0.0f;
    }
    float v = mov.velocidade + MOV_ACELERACAO_MM_S2 * DT_S;
    float v_freio = sqrtf(2.0f * MOV_ACELERACAO_MM_S2 * restante_mm);
    if (v > mov.velocidade_max) {
        v = mov.velocidade_max;
    }
    if (v > v_freio) {
        v = v_freio;
    }
    /* Sem um mínimo, a frenagem só chegaria ao total depois de muitos ciclos. */
    if (v < MOV_ACELERACAO_MM_S2 * DT_S) {
        v = MOV_ACELERACAO_MM_S2 * DT_S;
    }
    mov.referencia += v * DT_S / MM_POR_PULSO;
    if (mov.referencia >= total) {
        mov.referencia = total;
        v = 0.0f;
    }
    mov.aceleracao = (v - mov.velocidade) / DT_S;
    mov.velocidade = v;
}

mov_estado_t mov_atualizar(void)
{
    if (mov.estado != MOV_EM_CURSO) {
        return mov.estado;
    }

    float sinal_esq = mov.tipo == TIPO_GIRO ? -1.0f : 1.0f;
    float sinal_dir = 1.0f;
    float total = mov.total;
    if (total < 0.0f) {
        /* Giro para a direita: as duas rodas trocam de sentido. */
        sinal_esq = -sinal_esq;
        sinal_dir = -1.0f;
        total = -total;
    }

    float esquerda = sinal_esq * (float)(hal_encoder_ler(MOTOR_ESQ) - mov.inicio[MOTOR_ESQ]);
    float direita = sinal_dir * (float)(hal_encoder_ler(MOTOR_DIR) - mov.inicio[MOTOR_DIR]);

    if (mov.tipo == TIPO_AVANCO) {
        if (MOV_KP_CENTRO != 0.0f) {
            mov.desvio += MOV_KP_CENTRO * erro_lateral_mm();
        }
        if (MOV_GANHO_FRONTAL != 0.0f) {
            corrigir_pela_frente((esquerda + direita) / 2.0f);
            total = mov.total;
        }
    }

    avancar_referencia(total);

    float referencia_esq = mov.referencia - mov.desvio / 2.0f;
    float referencia_dir = mov.referencia + mov.desvio / 2.0f;
    float erro_esq = referencia_esq - esquerda;
    float erro_dir = referencia_dir - direita;

    /*
     * Potência de alimentação pela velocidade pedida, adiantada pelo atraso do motor
     * (sem isso a roda fica para trás na rampa e passa do ponto na frenagem),
     * mais a correção do erro de posição.
     */
    float alimentacao = (mov.velocidade + MOV_ATRASO_MOTOR_S * mov.aceleracao) /
                        VELOCIDADE_MAX_MM_S * MOTOR_POTENCIA_MAX;
    float potencia_esq = sinal_esq * (alimentacao + MOV_KP_POSICAO * erro_esq);
    float potencia_dir = sinal_dir * (alimentacao + MOV_KP_POSICAO * erro_dir);
    definir_potencia(potencia_esq, potencia_dir);
    if (roda_travada(potencia_esq, potencia_dir)) {
        desligar(MOV_FALHOU);
        return mov.estado;
    }

    mov.decorrido_ms += MOV_PERIODO_MS;
    bool no_alvo = mov.referencia >= total && fabsf(erro_esq) <= MOV_TOLERANCIA_PULSOS &&
                   fabsf(erro_dir) <= MOV_TOLERANCIA_PULSOS;
    mov.assentado_ms = no_alvo ? mov.assentado_ms + MOV_PERIODO_MS : 0;

    if (mov.assentado_ms >= MOV_ASSENTAR_MS) {
        desligar(MOV_CONCLUIDO);
    } else if (mov.decorrido_ms >= MOV_TEMPO_LIMITE_MS) {
        desligar(MOV_FALHOU);
    }
    return mov.estado;
}
