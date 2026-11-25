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
 * No giro a roda esquerda segue a mesma referência com o sinal trocado; na ré as
 * duas trocam de sinal; no arco cada roda anda a referência vezes o seu fator (a de
 * dentro menos, a de fora mais). O desvio entre as rodas é o rumo pedido pela centralização.
 */
static struct {
    mov_estado_t estado;
    tipo_t tipo;
    bool celula;               /* avanço de célula: centraliza, corrige pela frente e para na parede */
    int32_t inicio[MOTOR_QTD]; /* encoder no início do movimento */
    float total;               /* pulsos a andar por roda; negativo = ré (avanço) ou direita (giro) */
    float referencia;          /* onde a roda deveria estar agora, em pulsos */
    float velocidade;          /* da referência, em mm/s */
    float aceleracao;          /* da referência, em mm/s² */
    float velocidade_max;
    float desvio;              /* pulsos que a roda direita vai à frente da esquerda */
    float fator[MOTOR_QTD];    /* quanto cada roda anda por pulso da referência: 1 no avanço, R ± B/2 sobre R no arco */
    float giro_previsto_rad;   /* quanto o arco gira o robô de propósito (fora do rumo torto) */
    float andado;              /* pulsos já andados no avanço, média das rodas */
    uint32_t freando_ms;       /* parada de emergência: tempo de freio já passado */
    bool freando;
    bool freio_no_fim;         /* a parada de emergência veio perto do fim previsto: conta como chegada */
    float total_previsto;      /* o total do início do avanço: a correção frontal não estica além disso */
    float progresso;           /* andado na última vez que a roda avançou de verdade */
    uint32_t sem_progresso_ms; /* tempo sem avançar com o fim perto: encostou na parede */
    bool parou_na_parede;      /* o último avanço de célula terminou com o nariz na parede */
    float residuo_avanco;      /* pulsos que o último avanço de célula ficou do alvo; o próximo compensa */
    float residuo_giro;        /* o mesmo para o giro, com sinal (positivo = à esquerda) */
    float andado_na_leitura;   /* o andado na última leitura do ToF: o robô seguiu andando desde então */
    bool tof_novo;             /* chegou leitura dos ToF desde o último ciclo */
    float lateral_mm;          /* eixo à esquerda do meio do corredor (negativo = à direita) */
    float rumo_inicio;         /* rumo do robô em relação ao corredor no início do movimento (rad, + à esquerda) */
    float rumo_rad;            /* rumo agora: rumo_inicio mais o que os encoders giraram desde o início */
    float desde_medida_mm;     /* avanço desde a última leitura lateral usada */
    bool tem_anterior;         /* já houve uma leitura dos ToF neste movimento */
    float lateral_anterior;    /* estimativa na leitura anterior: a mediana do ToF chega com ~50 ms de atraso */
    float theta_anterior;
    int fora_do_portao;        /* leituras seguidas que discordaram da estimativa mais que o portão */
    float deriva;              /* rad por mm andado para a frente que os encoders não veem (aprendida) */
    float rumo_corrigido;      /* soma das correções do rumo pelas paredes neste avanço */
    int correcoes;             /* leituras laterais usadas neste avanço */
    float integral[MOTOR_QTD]; /* parcela integral da potência de cada roda */
    uint32_t decorrido_ms;
    uint32_t assentado_ms;
    int32_t ultimo[MOTOR_QTD];      /* última leitura do encoder, para ver a roda travada */
    int32_t ciclo_anterior[MOTOR_QTD]; /* leitura do encoder no ciclo anterior: a roda está parada? */
    uint32_t parada_ms[MOTOR_QTD];      /* tempo da roda sem nenhum pulso */
    uint32_t parado_ms[MOTOR_QTD];  /* tempo da roda sem contar pulsos com potência alta */
    mov_tof_t tof;
    char diag_tipo;            /* para o diagnóstico: tipo e pedido do movimento em curso */
    float diag_pedido;
    mov_diag_t diag;
    bool diag_novo;
} mov;

int32_t mov_pulsos_avanco(float mm)
{
    return (int32_t)lroundf(mm / MM_POR_PULSO);
}

static float pulsos_giro(float graus)
{
    /* Girando no lugar, cada roda anda o arco de raio BITOLA_MM / 2. */
    return PI_F * BITOLA_MM * graus / 360.0f / MM_POR_PULSO;
}

int32_t mov_pulsos_giro(int graus)
{
    return (int32_t)lroundf(pulsos_giro((float)graus));
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

/* Guarda o resumo do movimento que terminou agora (mov_diagnostico). */
static void registrar_fim(mov_fim_t fim)
{
    mov_diag_t *d = &mov.diag;
    d->tipo = mov.diag_tipo;
    d->pedido = mov.diag_pedido;
    d->fim = fim;
    d->duracao_ms = mov.decorrido_ms + mov.freando_ms;
    d->pulsos_esq = hal_encoder_ler(MOTOR_ESQ) - mov.inicio[MOTOR_ESQ];
    d->pulsos_dir = hal_encoder_ler(MOTOR_DIR) - mov.inicio[MOTOR_DIR];
    if (mov.tipo == TIPO_GIRO) {
        d->alvo_esq = (int32_t)lroundf(-mov.total);
        d->alvo_dir = (int32_t)lroundf(mov.total);
    } else {
        d->alvo_esq = (int32_t)lroundf(mov.total * mov.fator[MOTOR_ESQ]);
        d->alvo_dir = (int32_t)lroundf(mov.total * mov.fator[MOTOR_DIR]);
    }
    d->lateral_mm = mov.lateral_mm;
    d->rumo_graus = mov.rumo_rad * 180.0f / PI_F;
    d->deriva_graus_m = mov.deriva * 1000.0f * 180.0f / PI_F;
    d->tof = mov.tof;
    mov.diag_novo = true;
}

bool mov_diagnostico(mov_diag_t *d)
{
    if (!mov.diag_novo || d == NULL) {
        return false;
    }
    *d = mov.diag;
    mov.diag_novo = false;
    return true;
}

const char *mov_fim_nome(mov_fim_t fim)
{
    static const char *const NOMES[] = {"nenhum", "assentou", "perto", "parede", "freio_fim",
                                        "tempo_perto", "TEMPO", "TRAVOU", "FREIO"};
    return (unsigned)fim < sizeof(NOMES) / sizeof(NOMES[0]) ? NOMES[fim] : "?";
}

/* Corta a potência e põe a ponte H em standby até o próximo movimento. */
static void desligar(mov_estado_t estado)
{
    if (estado != MOV_CONCLUIDO) {
        mov.residuo_avanco = 0.0f;
        mov.residuo_giro = 0.0f;
    }
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

static void iniciar(tipo_t tipo, float pulsos, float velocidade_max)
{
    mov.tipo = tipo;
    mov.celula = false;
    mov.inicio[MOTOR_ESQ] = hal_encoder_ler(MOTOR_ESQ);
    mov.inicio[MOTOR_DIR] = hal_encoder_ler(MOTOR_DIR);
    mov.total = pulsos;
    mov.referencia = 0.0f;
    mov.velocidade = 0.0f;
    mov.aceleracao = 0.0f;
    mov.velocidade_max = velocidade_max;
    mov.desvio = 0.0f;
    mov.fator[MOTOR_ESQ] = 1.0f;
    mov.fator[MOTOR_DIR] = 1.0f;
    mov.giro_previsto_rad = 0.0f;
    mov.andado = 0.0f;
    mov.andado_na_leitura = 0.0f;
    mov.freando = false;
    mov.freio_no_fim = false;
    mov.total_previsto = fabsf(pulsos);
    mov.progresso = 0.0f;
    mov.sem_progresso_ms = 0;
    mov.parou_na_parede = false;
    mov.freando_ms = 0;
    mov.integral[MOTOR_ESQ] = 0.0f;
    mov.integral[MOTOR_DIR] = 0.0f;
    mov.decorrido_ms = 0;
    mov.assentado_ms = 0;
    for (int i = 0; i < MOTOR_QTD; i++) {
        mov.ultimo[i] = mov.inicio[i];
        mov.ciclo_anterior[i] = mov.inicio[i];
        mov.parada_ms[i] = 0;
        mov.parado_ms[i] = 0;
    }
    mov.tof = (mov_tof_t){0};
    mov.tof_novo = false;
    mov.rumo_inicio = mov.rumo_rad;
    mov.desde_medida_mm = 0.0f;
    mov.tem_anterior = false;
    mov.fora_do_portao = 0;
    mov.rumo_corrigido = 0.0f;
    mov.correcoes = 0;
    mov.estado = MOV_EM_CURSO;
    hal_motor_habilitar(true);
}

void mov_iniciar(void)
{
    hal_motor_iniciar();
    hal_encoder_iniciar();
    mov.estado = MOV_LIVRE;
    mov.residuo_avanco = 0.0f;
    mov.residuo_giro = 0.0f;
    mov.lateral_mm = 0.0f;
    mov.rumo_rad = 0.0f;
    mov.deriva = 0.0f;
    mov.tof = (mov_tof_t){0};
    mov.diag_novo = false;
}

bool mov_avancar_mm(float mm, bool celula)
{
    if (mov.estado == MOV_EM_CURSO || mm == 0.0f) {
        return false;
    }
    float pulsos = mm / MM_POR_PULSO;
    if (celula && mm > 0.0f) {
        pulsos += mov.residuo_avanco;
    }
    iniciar(TIPO_AVANCO, pulsos, VELOCIDADE_CRUZEIRO_MM_S);
    mov.celula = celula && mm > 0.0f;
    mov.diag_tipo = mov.celula ? 'C' : (mm < 0.0f ? 'R' : 'A');
    mov.diag_pedido = mm;
    return true;
}

bool mov_avancar_celula(void)
{
    return mov_avancar_mm((float)CELULA_MM, true);
}

bool mov_girar_graus(float graus)
{
    if (mov.estado == MOV_EM_CURSO || graus == 0.0f || fabsf(graus) > 360.0f) {
        return false;
    }
    /* O sinal fica no total da roda direita: positivo gira para a esquerda. */
    iniciar(TIPO_GIRO, pulsos_giro(graus) + mov.residuo_giro, MOV_VELOCIDADE_GIRO_MM_S);
    mov.diag_tipo = 'G';
    mov.diag_pedido = graus;
    return true;
}

bool mov_arco(float graus, float raio_mm)
{
    if (mov.estado == MOV_EM_CURSO || graus == 0.0f || fabsf(graus) > 180.0f || raio_mm <= BITOLA_MM / 2.0f) {
        return false;
    }
    float angulo = graus * PI_F / 180.0f;
    float lado = graus > 0.0f ? 1.0f : -1.0f; /* à esquerda, a roda esquerda é a de dentro */
    float eixo_mm = fabsf(angulo) * raio_mm;
    iniciar(TIPO_AVANCO, eixo_mm / MM_POR_PULSO, MOV_VELOCIDADE_GIRO_MM_S);
    mov.fator[MOTOR_ESQ] = (raio_mm - lado * BITOLA_MM / 2.0f) / raio_mm;
    mov.fator[MOTOR_DIR] = (raio_mm + lado * BITOLA_MM / 2.0f) / raio_mm;
    mov.giro_previsto_rad = angulo;
    mov.diag_tipo = 'O';
    mov.diag_pedido = graus;
    return true;
}

bool mov_girar(int graus)
{
    if (graus != 90 && graus != -90 && graus != 180) {
        return false;
    }
    return mov_girar_graus((float)graus);
}

void mov_parar(void)
{
    desligar(MOV_LIVRE);
}

mov_estado_t mov_estado(void)
{
    return mov.estado;
}

float mov_andado_mm(void)
{
    return mov.andado * MM_POR_PULSO;
}

float mov_restante_mm(void)
{
    if (mov.tipo != TIPO_AVANCO || mov.estado != MOV_EM_CURSO) {
        return 0.0f;
    }
    return (fabsf(mov.total) - mov.andado) * MM_POR_PULSO;
}

bool mov_parou_na_parede(void)
{
    return mov.parou_na_parede;
}

float mov_lateral_mm(void)
{
    return mov.lateral_mm;
}

void mov_definir_lateral_mm(float mm)
{
    mov.lateral_mm = mm;
}

float mov_rumo_rad(void)
{
    return mov.rumo_rad;
}

void mov_definir_rumo_rad(float rad)
{
    mov.rumo_rad = rad;
}

void mov_informar_tof(const mov_tof_t *tof)
{
    if (tof != NULL) {
        mov.tof = *tof;
        mov.andado_na_leitura = mov.andado;
        mov.tof_novo = true;
    }
}

/*
 * Posição lateral do eixo pelas paredes, com rumo theta (rad, positivo à esquerda). Cada lateral, a
 * (TOF_E_X_MM, ±TOF_E_Y_MM) do eixo e apontado para o seu lado, mede até a parede a CELULA_LIVRE_MEIA_MM
 * do meio. Só vale leitura com parede e dentro da faixa em que o sensor mede (TOF_E_MIN_CONFIAVEL_MM e
 * TOF_D_MIN_CONFIAVEL_MM, na leitura com o erro já descontado).
 * Devolve false se nenhuma serviu.
 */
static bool lateral_pelas_paredes(float theta, float *lateral)
{
    float c = cosf(theta);
    float s = sinf(theta);
    float soma = 0.0f;
    int n = 0;
    uint16_t e = mov.tof.esquerda_mm;
    uint16_t d = mov.tof.direita_mm;
    if (mov.tof.parede_esquerda && e >= TOF_E_MIN_CONFIAVEL_MM && e <= MOV_LATERAL_MAX_MM) {
        soma += CELULA_LIVRE_MEIA_MM - ((float)e + TOF_E_Y_MM) * c - TOF_E_X_MM * s;
        n++;
    }
    if (mov.tof.parede_direita && d >= TOF_D_MIN_CONFIAVEL_MM && d <= MOV_LATERAL_MAX_MM) {
        soma += -CELULA_LIVRE_MEIA_MM + ((float)d - TOF_D_Y_MM) * c - TOF_D_X_MM * s;
        n++;
    }
    if (n == 0) {
        return false;
    }
    *lateral = soma / (float)n;
    return true;
}

/*
 * Leitura perto demais para medir (abaixo de TOF_D_MIN_CONFIAVEL_MM no direito; o esquerdo mede bem até
 * TOF_E_MIN_CONFIAVEL_MM): ela não diz a distância, mas diz que a parede está no máximo a
 * 2 x bruta - TOF_MIN_CONFIAVEL_MM no direito (bancada: 2 cm vira 30, 3 cm vira 40), com bruta = leitura +
 * TOF_D_DESVIO_MM e o erro descontado de novo no fim, e a leitura no esquerdo. Isso
 * limita a lateral: perto da parede esquerda, a lateral é pelo menos *minimo*; perto da direita, no máximo
 * *maximo*. Sem isso, num corredor com parede de um lado só, o robô que deriva para ela fica sem medida.
 */
static void limites_pelas_paredes(float theta, float *minimo, float *maximo)
{
    float c = cosf(theta);
    float s = sinf(theta);
    uint16_t e = mov.tof.esquerda_mm;
    uint16_t d = mov.tof.direita_mm;
    *minimo = -1e9f;
    *maximo = 1e9f;
    if (mov.tof.parede_esquerda && e > 0 && e < TOF_E_MIN_CONFIAVEL_MM) {
        float distancia_max = (float)e;
        *minimo = CELULA_LIVRE_MEIA_MM - (distancia_max + TOF_E_Y_MM) * c - TOF_E_X_MM * s;
    }
    if (mov.tof.parede_direita && d > 0 && d < TOF_D_MIN_CONFIAVEL_MM) {
        float bruta = (float)d + (float)TOF_D_DESVIO_MM;
        float distancia_max = fmaxf(0.0f, 2.0f * bruta - (float)TOF_MIN_CONFIAVEL_MM - (float)TOF_D_DESVIO_MM);
        *maximo = -CELULA_LIVRE_MEIA_MM + (distancia_max - TOF_D_Y_MM) * c - TOF_D_X_MM * s;
    }
}

/* Corrige o rumo pela diferença entre a medida e a estimativa, por mm andado desde a leitura anterior. */
static void corrigir_rumo(float diferenca)
{
    if (mov.desde_medida_mm >= 5.0f) {
        float antes = mov.rumo_inicio;
        mov.rumo_inicio += MOV_RUMO_FUSAO * diferenca / mov.desde_medida_mm;
        mov.rumo_inicio = fmaxf(-MOV_RUMO_MAX_RAD, fminf(MOV_RUMO_MAX_RAD, mov.rumo_inicio));
        mov.rumo_corrigido += mov.rumo_inicio - antes;
        mov.correcoes++;
    }
}

/*
 * Centralização (FIRM-10), um observador alfa-beta no espaço: a posição lateral anda pela odometria com o
 * rumo estimado, e cada leitura nova das paredes corrige a lateral e o rumo pela diferença entre a medida e
 * a estimativa (uma roda que anda mais que a outra entorta o robô sem o encoder ver). A mediana do ToF
 * descreve onde o robô estava ~50 ms antes: a medida é comparada com a estimativa da leitura anterior, e a
 * correção vale para a estimativa de agora. O rumo pedido leva o eixo ao meio e cai a zero no fim do
 * avanço: o robô para alinhado com o corredor. O rumo pedido menos o do início vira desvio entre as rodas.
 */
static void centralizar(float giro_encoders, float avanco_mm, float restante_mm)
{
    giro_encoders += mov.deriva * mov.andado * MM_POR_PULSO;
    float theta = mov.rumo_inicio + giro_encoders;
    mov.lateral_mm += avanco_mm * sinf(theta);
    mov.desde_medida_mm += avanco_mm;
    if (mov.tof_novo) {
        float medida;
        bool valida = mov.tem_anterior && lateral_pelas_paredes(mov.theta_anterior, &medida);
        if (valida && fabsf(medida - mov.lateral_anterior) > MOV_CENTRO_PORTAO_MM) {
            /* Uma leitura isolada fora do portão é de poste ou de abertura; várias seguidas, não. */
            if (++mov.fora_do_portao >= MOV_CENTRO_PORTAO_LEITURAS) {
                mov.lateral_mm += medida - mov.lateral_anterior;
                mov.fora_do_portao = 0;
            }
            valida = false;
        } else if (valida) {
            mov.fora_do_portao = 0;
        }
        if (valida) {
            float diferenca = medida - mov.lateral_anterior;
            mov.lateral_mm += MOV_CENTRO_FUSAO * diferenca;
            corrigir_rumo(diferenca);
        } else if (mov.tem_anterior) {
            float minimo, maximo;
            limites_pelas_paredes(mov.theta_anterior, &minimo, &maximo);
            float diferenca = 0.0f;
            if (mov.lateral_anterior < minimo) {
                diferenca = minimo - mov.lateral_anterior;
            } else if (mov.lateral_anterior > maximo) {
                diferenca = maximo - mov.lateral_anterior;
            }
            if (diferenca != 0.0f) {
                mov.lateral_mm += diferenca;
                corrigir_rumo(diferenca);
            }
        }
        mov.desde_medida_mm = 0.0f;
        mov.tem_anterior = true;
        mov.lateral_anterior = mov.lateral_mm;
        mov.theta_anterior = mov.rumo_inicio + giro_encoders;
    }
    float fator = fmaxf(0.0f, fminf(1.0f, restante_mm / MOV_CENTRO_FIM_MM));
    float rumo = -MOV_CENTRO_GANHO_RAD_POR_MM * mov.lateral_mm;
    rumo = fmaxf(-MOV_CENTRO_RUMO_MAX_RAD, fminf(MOV_CENTRO_RUMO_MAX_RAD, rumo)) * fator;
    float alvo = (rumo - mov.rumo_inicio) * BITOLA_MM / MM_POR_PULSO;
    float passo = fmaxf(-MOV_RUMO_PASSO_PULSOS, fminf(MOV_RUMO_PASSO_PULSOS, alvo - mov.desvio));
    mov.desvio += passo;
}

/*
 * Correção frontal (FIRM-10): com a parede da célula de destino à frente e a leitura na faixa confiável,
 * o fim do avanço passa a seguir a distância medida. A leitura é de antes (filtro do ToF): conta a posição
 * do robô quando ela foi tirada.
 */
static void corrigir_pela_frente(void)
{
    uint16_t frente = mov.tof.frente_mm;
    if (!mov.tof_novo || !mov.tof.parede_frente || frente < MOV_FRONTAL_MIN_MM ||
        frente >= PAREDE_FRENTE_ENTRA_MM) {
        return;
    }
    float na_leitura_mm = mov.andado_na_leitura * MM_POR_PULSO - mov.velocidade * MOV_TOF_ATRASO_S;
    float fim_mm = na_leitura_mm + (float)frente - MOV_FRENTE_ALVO_MM;
    float total_mm = mov.total * MM_POR_PULSO;
    if (fabsf(fim_mm - total_mm) > MOV_FRONTAL_JANELA_MM) {
        return; /* parede de outra célula, ou leitura ruim */
    }
    mov.total += MOV_GANHO_FRONTAL * (fim_mm - total_mm) / MM_POR_PULSO;
    float maximo = mov.total_previsto + MOV_FRONTAL_EXTENSAO_MM / MM_POR_PULSO;
    if (mov.total > maximo) {
        mov.total = maximo;
    }
    if (mov.total < mov.andado) {
        mov.total = mov.andado;
    }
}

/* O rumo no fim do avanço: o do início, o que as rodas giraram e a deriva (que troca de lado na ré). */
static void fechar_rumo_do_avanco(float esquerda, float direita)
{
    float feito = (esquerda + direita) / 2.0f;
    float andado_mm = (mov.total < 0.0f ? -feito : feito) * MM_POR_PULSO;
    mov.rumo_rad = mov.rumo_inicio + (direita - esquerda) * MM_POR_PULSO / BITOLA_MM - mov.giro_previsto_rad +
                   mov.deriva * andado_mm;
    if (mov.celula && mov.correcoes >= 4 && andado_mm > 100.0f) {
        mov.deriva += MOV_DERIVA_APRENDIZADO * mov.rumo_corrigido / andado_mm;
        mov.deriva = fmaxf(-MOV_DERIVA_MAX_RAD_POR_MM, fminf(MOV_DERIVA_MAX_RAD_POR_MM, mov.deriva));
    }
}

/*
 * Encostou na parede no fim do avanço de célula: falta pouco, as duas rodas empurram com força e o robô não anda
 * há MOV_PAREDE_PARADO_MS. A roda parada contra a parede não chega a ser "travada" (a potência fica abaixo de
 * MOV_TRAVADO_POTENCIA) e, sem isto, o robô empurraria até o tempo limite e a corrida falharia. Com pouca força
 * não é parede, é a zona morta: a força continua subindo.
 */
static bool encostou_na_parede(float total, float potencia_esq, float potencia_dir)
{
    if (fabsf(potencia_esq) < MOV_PAREDE_POTENCIA || fabsf(potencia_dir) < MOV_PAREDE_POTENCIA) {
        mov.progresso = mov.andado;
        mov.sem_progresso_ms = 0;
        return false;
    }
    if (mov.andado - mov.progresso > 1.0f) {
        mov.progresso = mov.andado;
        mov.sem_progresso_ms = 0;
        return false;
    }
    mov.sem_progresso_ms += MOV_PERIODO_MS;
    float restante_mm = (total - mov.andado) * MM_POR_PULSO;
    float atraso_pulsos = mov.referencia - mov.andado;
    return restante_mm <= MOV_PAREDE_FIM_MM && atraso_pulsos > MOV_TOLERANCIA_AVANCO_PULSOS &&
           mov.sem_progresso_ms >= MOV_PAREDE_PARADO_MS;
}

/*
 * Parada de emergência: a distância até a parede da frente agora (a leitura, menos o que o robô andou desde ela e
 * menos o atraso do filtro do ToF) menos o que o robô ainda anda até parar tem que ser pelo menos
 * MOV_FRENTE_SOBRA_MM. Senão o avanço para, a tempo de sobrar essa distância.
 */
static bool obstaculo_a_frente(float andado_pulsos)
{
    if (mov.tof.frente_mm == 0) {
        return false;
    }
    float desde_a_leitura_mm = (andado_pulsos - mov.andado_na_leitura) * MM_POR_PULSO;
    float distancia_mm = (float)mov.tof.frente_mm - desde_a_leitura_mm - mov.velocidade * MOV_TOF_ATRASO_S;
    float frenagem_mm = mov.velocidade * mov.velocidade / (2.0f * MOV_ACELERACAO_MM_S2);
    return distancia_mm - frenagem_mm < (float)MOV_FRENTE_SOBRA_MM;
}

/*
 * Zona morta: a roda que não andou desde o ciclo anterior e ainda está longe do alvo recebe pelo menos
 * MOV_ZONA_MORTA, no sentido da potência pedida (ou do erro, se a potência for zero). Com a roda andando, nada muda.
 */
static float vencer_zona_morta(bool parada, float potencia, float erro, float tolerancia)
{
    if (!parada || fabsf(erro) <= tolerancia || fabsf(potencia) >= MOV_ZONA_MORTA) {
        return potencia;
    }
    float sinal = potencia != 0.0f ? potencia : erro;
    return sinal > 0.0f ? MOV_ZONA_MORTA : -MOV_ZONA_MORTA;
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

    if (mov.freando) {
        /* Freio em curto até as rodas pararem; só então solta a ponte H. */
        hal_motor_frear(MOTOR_ESQ);
        hal_motor_frear(MOTOR_DIR);
        mov.freando_ms += MOV_PERIODO_MS;
        if (mov.freando_ms >= MOV_FREIO_MS) {
            if (mov.freio_no_fim) {
                /* Parou pela parede já perto do fim previsto: chegou na célula. */
                mov.residuo_avanco = 0.0f;
                registrar_fim(MOV_FIM_FREIO_FIM);
                desligar(MOV_CONCLUIDO);
            } else {
                registrar_fim(MOV_FIM_FREIO);
                desligar(MOV_FALHOU);
            }
        }
        return mov.estado;
    }

    float sinal_esq = mov.tipo == TIPO_GIRO ? -1.0f : 1.0f;
    float sinal_dir = 1.0f;
    float total = mov.total;
    if (total < 0.0f) {
        /* Giro para a direita ou ré: as duas rodas trocam de sentido. */
        sinal_esq = -sinal_esq;
        sinal_dir = -1.0f;
        total = -total;
    }

    float esquerda = sinal_esq * (float)(hal_encoder_ler(MOTOR_ESQ) - mov.inicio[MOTOR_ESQ]);
    float direita = sinal_dir * (float)(hal_encoder_ler(MOTOR_DIR) - mov.inicio[MOTOR_DIR]);
    /* Roda parada desde o ciclo anterior: é ela que precisa vencer a zona morta. */
    bool parada[MOTOR_QTD];
    for (int i = 0; i < MOTOR_QTD; i++) {
        int32_t leitura = hal_encoder_ler(i);
        parada[i] = leitura == mov.ciclo_anterior[i];
        mov.ciclo_anterior[i] = leitura;
        mov.parada_ms[i] = parada[i] ? mov.parada_ms[i] + MOV_PERIODO_MS : 0;
    }

    if (mov.tipo == TIPO_AVANCO) {
        float anterior = mov.andado;
        mov.andado = (esquerda + direita) / 2.0f;
        if (mov.celula) {
            if (obstaculo_a_frente(mov.andado)) {
                mov.freando = true;
                mov.freio_no_fim = (total - mov.andado) * MM_POR_PULSO <= MOV_FRENTE_FIM_MM;
                hal_motor_frear(MOTOR_ESQ);
                hal_motor_frear(MOTOR_DIR);
                return mov.estado;
            }
            centralizar((direita - esquerda) * MM_POR_PULSO / BITOLA_MM, (mov.andado - anterior) * MM_POR_PULSO,
                        (total - mov.andado) * MM_POR_PULSO);
            if (MOV_GANHO_FRONTAL != 0.0f) {
                corrigir_pela_frente();
                total = mov.total;
            }
        }
    }
    mov.tof_novo = false;

    avancar_referencia(total);

    float referencia_esq = mov.referencia * mov.fator[MOTOR_ESQ] - mov.desvio / 2.0f;
    float referencia_dir = mov.referencia * mov.fator[MOTOR_DIR] + mov.desvio / 2.0f;
    float erro_esq = referencia_esq - esquerda;
    float erro_dir = referencia_dir - direita;

    /*
     * Potência de alimentação pela velocidade pedida, adiantada pelo atraso do motor
     * (sem isso a roda fica para trás na rampa e passa do ponto na frenagem),
     * mais a correção do erro de posição.
     */
    float alimentacao = (mov.velocidade + MOV_ATRASO_MOTOR_S * mov.aceleracao) /
                        VELOCIDADE_MAX_MM_S * MOTOR_POTENCIA_MAX;
    /*
     * A parcela integral vence o atrito do motor parado: sem ela, um erro pequeno no fim do movimento dá uma
     * potência menor que a zona morta do motor, e a roda para antes de chegar. Só acumula depois que a
     * trajetória terminou (durante o trajeto o erro é só o atraso da rampa) e só com a roda parada: quando ela
     * desgruda, a integral cai rápido, senão a roda dispara, passa do alvo e oscila (gruda e solta).
     */
    if (mov.referencia >= total) {
        const float erros[MOTOR_QTD] = {erro_esq, erro_dir};
        for (int i = 0; i < MOTOR_QTD; i++) {
            if (parada[i]) {
                mov.integral[i] = fmaxf(-MOV_INTEGRAL_MAX,
                                        fminf(MOV_INTEGRAL_MAX, mov.integral[i] + MOV_KI_POSICAO * erros[i] * DT_S));
            } else {
                mov.integral[i] *= MOV_INTEGRAL_SOLTA;
            }
        }
    }
    /* Dentro da tolerância a roda é solta: insistir contra o atrito a faz passar do ponto e oscilar. */
    bool trajetoria_fim = mov.referencia >= total;
    float tolerancia = mov.tipo == TIPO_AVANCO ? MOV_TOLERANCIA_AVANCO_PULSOS : MOV_TOLERANCIA_PULSOS;
    if (trajetoria_fim && fabsf(erro_esq) <= tolerancia) {
        mov.integral[MOTOR_ESQ] = 0.0f;
    }
    if (trajetoria_fim && fabsf(erro_dir) <= tolerancia) {
        mov.integral[MOTOR_DIR] = 0.0f;
    }
    float potencia_esq = sinal_esq * (alimentacao * mov.fator[MOTOR_ESQ] + MOV_KP_POSICAO * erro_esq + mov.integral[MOTOR_ESQ]);
    float potencia_dir = sinal_dir * (alimentacao * mov.fator[MOTOR_DIR] + MOV_KP_POSICAO * erro_dir + mov.integral[MOTOR_DIR]);
    if (trajetoria_fim) {
        potencia_esq = vencer_zona_morta(parada[MOTOR_ESQ], potencia_esq, erro_esq, tolerancia);
        potencia_dir = vencer_zona_morta(parada[MOTOR_DIR], potencia_dir, erro_dir, tolerancia);
    }
    if (trajetoria_fim && fabsf(erro_esq) <= tolerancia) {
        potencia_esq = 0.0f;
    }
    if (trajetoria_fim && fabsf(erro_dir) <= tolerancia) {
        potencia_dir = 0.0f;
    }
    definir_potencia(potencia_esq, potencia_dir);
    if (mov.tipo == TIPO_AVANCO && mov.celula && encostou_na_parede(total, potencia_esq, potencia_dir)) {
        fechar_rumo_do_avanco(esquerda, direita);
        mov.residuo_avanco = 0.0f;
        mov.parou_na_parede = true;
        registrar_fim(MOV_FIM_PAREDE);
        desligar(MOV_CONCLUIDO);
        return mov.estado;
    }
    if (roda_travada(potencia_esq, potencia_dir)) {
        registrar_fim(MOV_FIM_TRAVOU);
        desligar(MOV_FALHOU);
        return mov.estado;
    }

    mov.decorrido_ms += MOV_PERIODO_MS;
    bool no_alvo = mov.referencia >= total && fabsf(erro_esq) <= tolerancia &&
                   fabsf(erro_dir) <= tolerancia;
    mov.assentado_ms = no_alvo ? mov.assentado_ms + MOV_PERIODO_MS : 0;
    float perto = mov.tipo == TIPO_AVANCO ? MOV_PERTO_AVANCO_PULSOS : MOV_PERTO_GIRO_PULSOS;
    bool perto_e_parado = trajetoria_fim && fabsf(erro_esq) <= perto && fabsf(erro_dir) <= perto &&
                          mov.parada_ms[MOTOR_ESQ] >= MOV_PERTO_PARADO_MS &&
                          mov.parada_ms[MOTOR_DIR] >= MOV_PERTO_PARADO_MS;

    if (mov.assentado_ms >= MOV_ASSENTAR_MS || perto_e_parado) {
        /* O que faltou (ou passou) do alvo entra no próximo movimento: o erro de cada um não se soma. */
        float feito = (esquerda + direita) / 2.0f;
        if (mov.tipo == TIPO_AVANCO) {
            fechar_rumo_do_avanco(esquerda, direita);
        }
        if (mov.tipo == TIPO_GIRO) {
            mov.residuo_giro = (mov.total < 0.0f ? -1.0f : 1.0f) * (total - feito);
        } else if (mov.celula) {
            mov.residuo_avanco = total - feito;
        }
        registrar_fim(mov.assentado_ms >= MOV_ASSENTAR_MS ? MOV_FIM_ASSENTOU : MOV_FIM_PERTO);
        desligar(MOV_CONCLUIDO);
    } else if (mov.decorrido_ms >= MOV_TEMPO_LIMITE_MS) {
        /* Tempo esgotado perto do alvo (zona morta): chega assim, e o resto vai para o movimento seguinte. */
        float limite = 2.5f * perto;
        if (trajetoria_fim && fabsf(erro_esq) <= limite && fabsf(erro_dir) <= limite) {
            float feito = (esquerda + direita) / 2.0f;
            if (mov.tipo == TIPO_AVANCO) {
                fechar_rumo_do_avanco(esquerda, direita);
            }
            if (mov.tipo == TIPO_GIRO) {
                mov.residuo_giro = (mov.total < 0.0f ? -1.0f : 1.0f) * (total - feito);
            } else if (mov.celula) {
                mov.residuo_avanco = total - feito;
            }
            registrar_fim(MOV_FIM_TEMPO_PERTO);
            desligar(MOV_CONCLUIDO);
        } else {
            registrar_fim(MOV_FIM_TEMPO);
            desligar(MOV_FALHOU);
        }
    }
    return mov.estado;
}
