#include "sim_mundo.h"

#include <math.h>
#include <string.h>

#include "config/robo.h"

#define GRAUS_PARA_RAD(g) ((g) * PI_F / 180.0f)

#define CONSTANTE_TEMPO_MOTOR_S  0.05f  /* atraso da roda para atingir a velocidade pedida */
#define MEIA_PAREDE_MM           (PAREDE_ESPESSURA_MM / 2.0f)
#define PASSO_CONTORNO_MM        5.0f   /* distância entre os pontos do contorno testados na colisão */

/* Consumo simulado, só para a tensão cair de forma plausível */
#define DESCARGA_REPOUSO_MV_S    0.1f
#define DESCARGA_MOTORES_MV_S    0.5f   /* a mais, com os dois motores em potência máxima */
#define BATERIA_MINIMA_MV        6000.0f

typedef struct {
    float x_mm, y_mm, ang_graus;
} posicao_tof_t;

static const posicao_tof_t POSICAO_TOF[SIM_TOF_QTD] = {
    {TOF_F_X_MM, TOF_F_Y_MM, TOF_F_ANG},
    {TOF_E_X_MM, TOF_E_Y_MM, TOF_E_ANG},
    {TOF_D_X_MM, TOF_D_Y_MM, TOF_D_ANG},
};

static struct {
    sim_labirinto_t lab;
    sim_pose_t pose;
    uint32_t tempo_ms;
    bool motores_habilitados;
    int16_t potencia[2];
    int16_t zona_morta;
    float escala_roda[2]; /* 0 = 1: quanto a roda realmente anda por pulso contado */
    float velocidade_mm_s[2];
    float pulsos[2];
    bool encoder_travado[2];
    bool rodas_no_ar;
    double bateria_mv; /* double: a descarga de 1 ms some na precisão de um float */
    bool tof_ausente[SIM_TOF_QTD];
    uint16_t tof_ruido_mm;
    uint32_t tof_semente;
    bool tof_tem_pico[SIM_TOF_QTD];
    bool tof_curto; /* perto da parede o sensor lê de 30 a 40 mm, como o VL53L0X do robô */
    int16_t tof_desvio[SIM_TOF_QTD]; /* erro somado à leitura, como um sensor descalibrado */
    uint16_t tof_pico_mm[SIM_TOF_QTD];
    bool boot;
    bool led;
    bool buzzer;
} m;

/* true se o ponto (em mm) cai dentro de uma parede, de um poste ou fora do labirinto. */
static bool ponto_em_parede(float px, float py)
{
    float largura_mm = m.lab.largura * (float)CELULA_MM;
    float altura_mm = m.lab.altura * (float)CELULA_MM;

    if (px < 0 || py < 0 || px >= largura_mm || py >= altura_mm) {
        return true;
    }

    int cx = (int)(px / CELULA_MM);
    int cy = (int)(py / CELULA_MM);
    float lx = px - cx * (float)CELULA_MM;
    float ly = py - cy * (float)CELULA_MM;
    bool perto_o = lx < MEIA_PAREDE_MM;
    bool perto_l = lx > CELULA_MM - MEIA_PAREDE_MM;
    bool perto_s = ly < MEIA_PAREDE_MM;
    bool perto_n = ly > CELULA_MM - MEIA_PAREDE_MM;

    /* Os postes dos cantos existem sempre, com ou sem parede. */
    if ((perto_o || perto_l) && (perto_s || perto_n)) {
        return true;
    }
    return (perto_o && sim_labirinto_tem_parede(&m.lab, cx, cy, SIM_PAREDE_O)) ||
           (perto_l && sim_labirinto_tem_parede(&m.lab, cx, cy, SIM_PAREDE_L)) ||
           (perto_s && sim_labirinto_tem_parede(&m.lab, cx, cy, SIM_PAREDE_S)) ||
           (perto_n && sim_labirinto_tem_parede(&m.lab, cx, cy, SIM_PAREDE_N));
}

/*
 * O contorno do robô montado é um retângulo em volta do eixo (config/robo.h): da traseira à frente e de uma
 * lateral à outra. Testa pontos a cada PASSO_CONTORNO_MM ao longo das quatro bordas, na pose (x, y, theta).
 */
static bool robo_encosta_em_parede(float x, float y, float theta)
{
    const float cantos[4][2] = {
        {EIXO_ATE_FRENTE_MM, MEIA_LARGURA_ESQ_MM},
        {-EIXO_ATE_TRAS_MM, MEIA_LARGURA_ESQ_MM},
        {-EIXO_ATE_TRAS_MM, -MEIA_LARGURA_DIR_MM},
        {EIXO_ATE_FRENTE_MM, -MEIA_LARGURA_DIR_MM},
    };
    float c = cosf(theta);
    float s = sinf(theta);
    for (int lado = 0; lado < 4; lado++) {
        const float *a = cantos[lado];
        const float *b = cantos[(lado + 1) % 4];
        float comprimento = fabsf(b[0] - a[0]) + fabsf(b[1] - a[1]);
        int pontos = (int)(comprimento / PASSO_CONTORNO_MM) + 1;
        for (int i = 0; i < pontos; i++) {
            float t = (float)i / (float)pontos;
            float lx = a[0] + (b[0] - a[0]) * t;
            float ly = a[1] + (b[1] - a[1]) * t;
            if (ponto_em_parede(x + lx * c - ly * s, y + lx * s + ly * c)) {
                return true;
            }
        }
    }
    return false;
}

/* Pose do eixo para o chassi centrado no ponto (cx, cy), virado para theta. */
static void pose_centrada(float cx, float cy, float theta)
{
    sim_definir_pose(cx - EIXO_RECUO_MM * cosf(theta), cy - EIXO_RECUO_MM * sinf(theta), theta);
}

bool sim_mundo_iniciar(const char *labirinto_texto)
{
    memset(&m, 0, sizeof(m));
    if (!sim_labirinto_carregar(&m.lab, labirinto_texto)) {
        return false;
    }
    pose_centrada(CELULA_MM / 2.0f, CELULA_MM / 2.0f, PI_F / 2.0f);
    m.bateria_mv = BATERIA_CHEIA_MV;
    return true;
}

static void passo_1ms(void)
{
    const float dt = 0.001f;
    float deslocamento[2];

    for (int i = 0; i < 2; i++) {
        bool abaixo_da_zona = m.potencia[i] > -m.zona_morta && m.potencia[i] < m.zona_morta;
        float alvo = m.motores_habilitados && !abaixo_da_zona
                         ? VELOCIDADE_MAX_MM_S * m.potencia[i] / 1000.0f
                         : 0.0f;
        m.velocidade_mm_s[i] += (alvo - m.velocidade_mm_s[i]) * dt / CONSTANTE_TEMPO_MOTOR_S;
        deslocamento[i] = m.velocidade_mm_s[i] * dt;
    }
    float fisico[2];
    for (int i = 0; i < 2; i++) {
        fisico[i] = deslocamento[i] * (m.escala_roda[i] > 0.0f ? m.escala_roda[i] : 1.0f);
    }

    /* Cinemática da tração diferencial: média das rodas avança, diferença gira. */
    float avanco = (fisico[0] + fisico[1]) / 2.0f;
    float giro = (fisico[1] - fisico[0]) / BITOLA_MM;
    float theta_medio = m.pose.theta_rad + giro / 2.0f;
    float novo_x = m.pose.x_mm + avanco * cosf(theta_medio);
    float novo_y = m.pose.y_mm + avanco * sinf(theta_medio);

    if (m.rodas_no_ar) {
        /* Rodas no ar (autoteste do health-check): giram e contam, mas o robô não sai do lugar. */
        m.pose.colidiu = false;
        for (int i = 0; i < 2; i++) {
            if (!m.encoder_travado[i]) {
                m.pulsos[i] += deslocamento[i] / MM_POR_PULSO;
            }
        }
    } else if (robo_encosta_em_parede(novo_x, novo_y, m.pose.theta_rad + giro)) {
        /* Bateu: o robô não sai do lugar e as rodas (e os encoders) param. */
        m.pose.colidiu = true;
        m.velocidade_mm_s[0] = m.velocidade_mm_s[1] = 0.0f;
    } else {
        m.pose.colidiu = false;
        m.pose.x_mm = novo_x;
        m.pose.y_mm = novo_y;
        m.pose.theta_rad += giro;
        for (int i = 0; i < 2; i++) {
            if (!m.encoder_travado[i]) {
                m.pulsos[i] += deslocamento[i] / MM_POR_PULSO;
            }
        }
    }

    float uso_motores = (fabsf((float)m.potencia[0]) + fabsf((float)m.potencia[1])) / 2000.0f;
    if (!m.motores_habilitados) {
        uso_motores = 0.0f;
    }
    m.bateria_mv -= (double)(DESCARGA_REPOUSO_MV_S + DESCARGA_MOTORES_MV_S * uso_motores) * dt;
    if (m.bateria_mv < BATERIA_MINIMA_MV) {
        m.bateria_mv = BATERIA_MINIMA_MV;
    }

    m.tempo_ms++;
}

void sim_mundo_avancar(uint32_t dt_ms)
{
    for (uint32_t i = 0; i < dt_ms; i++) {
        passo_1ms();
    }
}

uint32_t sim_tempo_ms(void) { return m.tempo_ms; }

void sim_definir_rodas_no_ar(bool no_ar) { m.rodas_no_ar = no_ar; }

void sim_motor_habilitar(bool habilitar) { m.motores_habilitados = habilitar; }

void sim_definir_zona_morta(int16_t potencia) { m.zona_morta = potencia; }

void sim_definir_escala_roda(int motor, float fator)
{
    if (motor >= 0 && motor < 2) {
        m.escala_roda[motor] = fator;
    }
}

void sim_motor_definir(int motor, int16_t potencia)
{
    if (potencia > 1000) {
        potencia = 1000;
    } else if (potencia < -1000) {
        potencia = -1000;
    }
    m.potencia[motor] = potencia;
}

/* Trunca em direção ao zero, para a frente e a ré contarem igual. */
int32_t sim_encoder(int motor) { return (int32_t)m.pulsos[motor]; }

/* Gerador congruente linear (Numerical Recipes): basta para ruído de teste. */
static uint32_t proximo_aleatorio(void)
{
    m.tof_semente = m.tof_semente * 1664525u + 1013904223u;
    return m.tof_semente;
}

static int aplicar_ruido(int d)
{
    if (m.tof_ruido_mm == 0) {
        return d;
    }
    int faixa = 2 * m.tof_ruido_mm + 1;
    int erro = (int)(proximo_aleatorio() >> 16) % faixa - m.tof_ruido_mm;
    return d + erro < 0 ? 0 : d + erro;
}

bool sim_tof_medir(int tof, uint16_t *distancia_mm)
{
    if (tof < 0 || tof >= SIM_TOF_QTD || m.tof_ausente[tof]) {
        return false;
    }
    if (m.tof_tem_pico[tof]) {
        m.tof_tem_pico[tof] = false;
        *distancia_mm = m.tof_pico_mm[tof];
        return true;
    }

    /* Posição e direção do sensor no labirinto, a partir da pose do robô. */
    const posicao_tof_t *p = &POSICAO_TOF[tof];
    float c = cosf(m.pose.theta_rad);
    float s = sinf(m.pose.theta_rad);
    float ox = m.pose.x_mm + p->x_mm * c - p->y_mm * s;
    float oy = m.pose.y_mm + p->x_mm * s + p->y_mm * c;
    float direcao = m.pose.theta_rad + GRAUS_PARA_RAD(p->ang_graus);
    float dx = cosf(direcao);
    float dy = sinf(direcao);

    /* Anda 1 mm por vez até encontrar uma parede. */
    for (int d = 0; d <= TOF_ALCANCE_MAX_MM; d++) {
        if (ponto_em_parede(ox + d * dx, oy + d * dy)) {
            d += m.tof_desvio[tof];
            if (d < 0) {
                d = 0;
            }
            /*
             * Os três comprimem de perto (o esquerdo também: medido em 25/11/2026). A compressão é do sensor, na
             * leitura bruta (antes do desconto do erro, TOF_*_DESVIO_MM): o mundo devolve a leitura já
             * descontada, como o driver real.
             */
            if (m.tof_curto) {
                int desvio = tof == 0 ? TOF_F_DESVIO_MM : tof == 1 ? TOF_E_DESVIO_MM : TOF_D_DESVIO_MM;
                int bruto = d + desvio;
                if (bruto < TOF_MIN_CONFIAVEL_MM) {
                    /* bruto de 20 mm vira 30, 30 vira 35: perto, a leitura não mede */
                    d = TOF_MIN_CONFIAVEL_MM / 2 + bruto / 2 - desvio;
                }
            }
            *distancia_mm = (uint16_t)aplicar_ruido(d);
            return true;
        }
    }
    *distancia_mm = SIM_TOF_FORA_DE_ALCANCE;
    return true;
}

/* o divisor fica depois do diodo SS34: o ADC vê a bateria menos a queda */
uint16_t sim_bateria_adc_mv(void) { return (uint16_t)((m.bateria_mv - BATERIA_QUEDA_DIODO_MV) / BATERIA_DIVISOR); }

bool sim_boot_pressionado(void) { return m.boot; }
void sim_led_definir(bool aceso) { m.led = aceso; }
void sim_buzzer_definir(bool ligado) { m.buzzer = ligado; }

void sim_definir_tof_ausente(int tof, bool ausente) { m.tof_ausente[tof] = ausente; }

void sim_travar_encoder(int motor, bool travado)
{
    if (motor >= 0 && motor < 2) {
        m.encoder_travado[motor] = travado;
    }
}
void sim_definir_tof_ruido(uint16_t amplitude_mm, uint32_t semente)
{
    m.tof_ruido_mm = amplitude_mm;
    m.tof_semente = semente;
}

void sim_definir_tof_curto(bool ligado) { m.tof_curto = ligado; }

void sim_definir_tof_desvio(int tof, int16_t mm)
{
    if (tof >= 0 && tof < SIM_TOF_QTD) {
        m.tof_desvio[tof] = mm;
    }
}

void sim_definir_tof_pico(int tof, uint16_t distancia_mm)
{
    m.tof_tem_pico[tof] = true;
    m.tof_pico_mm[tof] = distancia_mm;
}

void sim_definir_bateria_mv(uint16_t mv) { m.bateria_mv = mv; }
void sim_definir_boot(bool pressionado) { m.boot = pressionado; }

static const float THETA_DIRECAO[4] = {PI_F / 2.0f, 0.0f, -PI_F / 2.0f, PI_F};

/*
 * Robô parado numa célula, onde a navegação lê as paredes: com os laterais a 90°, é a mesma posição do
 * centro (chassi centrado). Antes, com os laterais a 45°, a leitura era logo depois de entrar na célula.
 */
void sim_mundo_na_celula(int celula_x, int celula_y, int direcao)
{
    sim_mundo_no_centro_da_celula(celula_x, celula_y, direcao);
}

void sim_mundo_no_centro_da_celula(int celula_x, int celula_y, int direcao)
{
    pose_centrada(((float)celula_x + 0.5f) * (float)CELULA_MM, ((float)celula_y + 0.5f) * (float)CELULA_MM,
                  THETA_DIRECAO[direcao >= 0 && direcao < 4 ? direcao : 0]);
}

sim_pose_t sim_centro_do_chassi(void)
{
    sim_pose_t p = m.pose;
    p.x_mm += EIXO_RECUO_MM * cosf(p.theta_rad);
    p.y_mm += EIXO_RECUO_MM * sinf(p.theta_rad);
    return p;
}

void sim_definir_pose(float x_mm, float y_mm, float theta_rad)
{
    m.pose.x_mm = x_mm;
    m.pose.y_mm = y_mm;
    m.pose.theta_rad = theta_rad;
    m.pose.colidiu = false;
}

sim_pose_t sim_pose(void) { return m.pose; }
bool sim_motores_habilitados(void) { return m.motores_habilitados; }
bool sim_led_aceso(void) { return m.led; }
bool sim_buzzer_ligado(void) { return m.buzzer; }
const sim_labirinto_t *sim_labirinto(void) { return &m.lab; }
