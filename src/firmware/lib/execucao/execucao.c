#include "execucao.h"

#include <math.h>

#include "config/robo.h"

/*
 * Cada ação vira uma sequência de etapas (avanço, ré ou giro), calculada pela geometria do robô montado
 * (docs/decisoes/024). Referências: com o chassi centrado na célula, o eixo fica EIXO_RECUO_MM atrás do
 * centro. A posição do eixo é escrita como (longitudinal, lateral) no referencial do rumo atual, em mm a
 * partir do centro da célula; a lateral é positiva à esquerda.
 */

#define ETAPAS_MAX 8
#define GRAUS_PARA_RAD(g) ((g) * PI_F / 180.0f)

typedef enum { ETAPA_AVANCO, ETAPA_GIRO, ETAPA_ARCO } etapa_tipo_t;

typedef struct {
    etapa_tipo_t tipo;
    float valor;    /* mm (negativo = ré) ou graus (positivo = esquerda) no giro e no arco */
    bool celula;    /* avanço que termina na célula nova: centraliza pelas paredes */
    float lateral;  /* posição lateral do eixo no início da etapa (só no avanço de célula) */
} etapa_t;

static struct {
    exec_estado_t estado;
    etapa_t etapas[ETAPAS_MAX];
    int quantidade;
    int atual;
    /*
     * Onde o eixo parou na célula, longitudinal (mm a partir do centro): -EIXO_RECUO_MM no repouso normal, ou
     * mais à frente se o avanço terminou encostado na parede. A ação seguinte parte daí.
     */
    float longitudinal;
} exec;

/* Raio de um canto do robô a partir do eixo. */
static float raio(float x, float y)
{
    return sqrtf(x * x + y * y);
}

static void reiniciar(void)
{
    exec.estado = EXEC_OCIOSO;
    exec.quantidade = 0;
    exec.atual = 0;
}

void exec_iniciar(void)
{
    reiniciar();
    exec.longitudinal = -EIXO_RECUO_MM;
    mov_definir_lateral_mm(0.0f);
    mov_definir_rumo_rad(0.0f);
}

bool exec_eh_movimento(nav_acao_t acao)
{
    return acao == NAV_ACAO_FRENTE || acao == NAV_ACAO_DIREITA || acao == NAV_ACAO_ESQUERDA ||
           acao == NAV_ACAO_MEIA_VOLTA;
}

static void adicionar(etapa_tipo_t tipo, float valor, bool celula, float lateral)
{
    if (exec.quantidade < ETAPAS_MAX) {
        exec.etapas[exec.quantidade++] = (etapa_t){tipo, valor, celula, lateral};
    }
}

/* Avanço até o repouso da célula seguinte, partindo do eixo em (longitudinal, lateral). */
static void avancar_ate_a_celula(float longitudinal, float lateral)
{
    adicionar(ETAPA_AVANCO, (float)CELULA_MM - EIXO_RECUO_MM - longitudinal, true, lateral);
}

/*
 * Giro de 90° (sinal +1 à esquerda, -1 à direita) em arco: girar no lugar em volta do eixo, que fica atrás
 * do centro, passaria a quina a ~7 mm da parede da frente e do poste do lado aberto, e deixaria o robô
 * ~22 mm fora do meio. O robô recua até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro da célula e faz um
 * quarto de círculo desse raio: o centro do arco fica sobre a lateral, e o eixo termina no meio do corredor
 * novo, na fronteira da célula seguinte. A lateral de antes vira erro longitudinal, que o avanço absorve.
 */
static void planejar_giro_90(int sinal, float lateral, float longitudinal)
{
    /* Recua do ponto em que parou até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro. */
    float recuo = GIRO_RAIO_ARCO_MM + longitudinal;
    if (recuo >= 1.0f) {
        adicionar(ETAPA_AVANCO, -recuo, false, 0.0f);
    }
    adicionar(ETAPA_ARCO, 90.0f * (float)sinal, false, 0.0f);
    /* Fim do arco: longitudinal' = R + lateral (à esquerda) ou R - lateral (à direita); lateral' = 0. */
    avancar_ate_a_celula(GIRO_RAIO_ARCO_MM + (float)sinal * lateral, 0.0f);
}

/*
 * Meia-volta: o giro de 180° no lugar não cabe num corredor (as quinas da frente varrem ~100 mm para os
 * dois lados, e do meio à parede há 82,5). Ele cabe com o eixo deslocado para o lado contrário ao das
 * quinas da frente quando elas passam pela lateral: entre (raio_frente - L) e (L - raio_tras) do meio,
 * para o lado de fora do giro. A manobra: gira GIRO_ALFA, anda (de ré, quase sempre) o que leva o eixo a
 * essa faixa, e completa os 180°. Gira para o lado em que o eixo já está, para andar menos.
 */
static void planejar_meia_volta(float lateral, float longitudinal_inicial)
{
    /* Encostado na parede, a quina não gira: primeiro volta ao repouso normal. */
    if (longitudinal_inicial > -EIXO_RECUO_MM + 1.0f) {
        adicionar(ETAPA_AVANCO, -(longitudinal_inicial + EIXO_RECUO_MM), false, 0.0f);
    }
    float raio_frente = raio(EIXO_ATE_FRENTE_MM, fmaxf(MEIA_LARGURA_ESQ_MM, MEIA_LARGURA_DIR_MM));
    float raio_tras = raio(EIXO_ATE_TRAS_MM, fmaxf(MEIA_LARGURA_ESQ_MM, MEIA_LARGURA_DIR_MM));
    float deslocado = (raio_frente - raio_tras) / 2.0f; /* meio da faixa: ~22 mm */
    /* Eixo à esquerda (lateral >= 0): gira para a direita, com o eixo à esquerda; e vice-versa. */
    float sinal = lateral >= 0.0f ? -1.0f : 1.0f;
    float alvo = -sinal * deslocado;
    float alfa = GIRO_ALFA_MEIA_VOLTA_GRAUS * sinal;
    /* Andando t no rumo alfa, a lateral muda t x sen(alfa). */
    float t = (alvo - lateral) / sinf(GRAUS_PARA_RAD(alfa));
    /* Para a frente, só um pouco: pode haver parede à frente (beco). */
    t = fminf(t, 10.0f);
    float lateral_girando = lateral + t * sinf(GRAUS_PARA_RAD(alfa));
    float longitudinal = -EIXO_RECUO_MM + t * cosf(GRAUS_PARA_RAD(alfa));

    adicionar(ETAPA_GIRO, alfa, false, 0.0f);
    if (fabsf(t) >= 1.0f) {
        adicionar(ETAPA_AVANCO, t, false, 0.0f);
    }
    adicionar(ETAPA_GIRO, sinal * 180.0f - alfa, false, 0.0f);
    /* Rumo invertido: longitudinal' = -longitudinal, lateral' = -lateral. */
    longitudinal = -longitudinal;
    lateral = -lateral_girando;

    /*
     * O eixo ficou ~22 mm fora do meio. Volta para o meio com um "S": gira GIRO_ALFA para o meio, anda na
     * diagonal e desgira. A diagonal para antes de o nariz chegar perto da parede da célula seguinte (que
     * pode existir): o que sobrar de lateral, o avanço corrige pelas paredes.
     */
    if (fabsf(lateral) >= 5.0f) {
        float volta = lateral > 0.0f ? -GIRO_ALFA_MEIA_VOLTA_GRAUS : GIRO_ALFA_MEIA_VOLTA_GRAUS;
        float diagonal = -lateral / sinf(GRAUS_PARA_RAD(volta));
        float longitudinal_max = (float)CELULA_MM + CELULA_LIVRE_MEIA_MM - EIXO_ATE_FRENTE_MM - GIRO_FOLGA_FRENTE_MM;
        float diagonal_max = (longitudinal_max - longitudinal) / cosf(GRAUS_PARA_RAD(volta));
        diagonal = fminf(diagonal, diagonal_max);
        if (diagonal >= 1.0f) {
            adicionar(ETAPA_GIRO, volta, false, 0.0f);
            adicionar(ETAPA_AVANCO, diagonal, false, 0.0f);
            adicionar(ETAPA_GIRO, -volta, false, 0.0f);
            longitudinal += diagonal * cosf(GRAUS_PARA_RAD(volta));
            lateral += diagonal * sinf(GRAUS_PARA_RAD(volta));
        }
    }
    float resto = (float)CELULA_MM - EIXO_RECUO_MM - longitudinal;
    if (resto >= 1.0f) {
        avancar_ate_a_celula(longitudinal, lateral);
    } else if (resto <= -1.0f) {
        adicionar(ETAPA_AVANCO, resto, false, 0.0f); /* passou um pouco: ré até o repouso */
    }
}

static bool comecar_etapa(void)
{
    const etapa_t *e = &exec.etapas[exec.atual];
    if (e->tipo == ETAPA_GIRO || e->tipo == ETAPA_ARCO) {
        /* Desconta o rumo torto que o avanço estimou: o giro termina alinhado com o corredor novo. */
        float graus = e->valor - mov_rumo_rad() * 180.0f / PI_F;
        bool comecou = e->tipo == ETAPA_GIRO ? mov_girar_graus(graus) : mov_arco(graus, GIRO_RAIO_ARCO_MM);
        if (!comecou) {
            return false;
        }
        mov_definir_rumo_rad(0.0f);
        exec.estado = EXEC_GIRANDO;
        return true;
    }
    if (e->celula) {
        mov_definir_lateral_mm(e->lateral);
    }
    if (!mov_avancar_mm(e->valor, e->celula)) {
        return false;
    }
    exec.estado = EXEC_AVANCANDO;
    return true;
}

bool exec_comecar(nav_acao_t acao)
{
    if (!exec_eh_movimento(acao) || exec.estado == EXEC_GIRANDO || exec.estado == EXEC_AVANCANDO) {
        return false;
    }
    reiniciar();
    float lateral = mov_lateral_mm();
    switch (acao) {
    case NAV_ACAO_ESQUERDA:
        planejar_giro_90(1, lateral, exec.longitudinal);
        break;
    case NAV_ACAO_DIREITA:
        planejar_giro_90(-1, lateral, exec.longitudinal);
        break;
    case NAV_ACAO_MEIA_VOLTA:
        planejar_meia_volta(lateral, exec.longitudinal);
        break;
    default:
        /* Em frente: do ponto em que parou até o repouso da célula seguinte (CELULA_MM no repouso normal). */
        avancar_ate_a_celula(exec.longitudinal, lateral);
        break;
    }
    if (!comecar_etapa()) {
        reiniciar();
        return false;
    }
    return true;
}

exec_estado_t exec_atualizar(void)
{
    if (exec.estado != EXEC_GIRANDO && exec.estado != EXEC_AVANCANDO) {
        return exec.estado;
    }
    mov_estado_t m = mov_atualizar();
    if (m == MOV_FALHOU) {
        exec.estado = EXEC_FALHOU;
    } else if (m == MOV_CONCLUIDO) {
        bool ultima_celula = exec.etapas[exec.atual].celula;
        exec.atual++;
        if (exec.atual >= exec.quantidade) {
            exec.longitudinal = ultima_celula && mov_parou_na_parede()
                                    ? CELULA_LIVRE_MEIA_MM - EIXO_ATE_FRENTE_MM
                                    : -EIXO_RECUO_MM;
            exec.estado = EXEC_OCIOSO;
        } else if (!comecar_etapa()) {
            exec.estado = EXEC_FALHOU;
        }
    }
    return exec.estado;
}

exec_estado_t exec_estado(void)
{
    return exec.estado;
}

void exec_parar(void)
{
    mov_parar();
    reiniciar();
    exec.longitudinal = -EIXO_RECUO_MM;
    mov_definir_lateral_mm(0.0f);
    mov_definir_rumo_rad(0.0f);
}
