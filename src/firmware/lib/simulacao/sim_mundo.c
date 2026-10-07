#include "sim_mundo.h"

#include <math.h>
#include <string.h>

#include "config/robo.h"

#define PI_F 3.14159265f
#define GRAUS_PARA_RAD(g) ((g) * PI_F / 180.0f)

#define CONSTANTE_TEMPO_MOTOR_S  0.05f  /* atraso da roda para atingir a velocidade pedida */
#define MM_POR_PULSO             (PI_F * RODA_DIAMETRO_MM / ENCODER_PULSOS_POR_VOLTA)
#define MEIA_PAREDE_MM           (PAREDE_ESPESSURA_MM / 2.0f)
#define PONTOS_CONTORNO          16     /* pontos do contorno do robô testados na colisão */

/* Consumo simulado, só para a tensão cair de forma plausível */
#define DESCARGA_REPOUSO_MV_S    0.1f
#define DESCARGA_MOTORES_MV_S    0.5f   /* a mais, com os dois motores em potência máxima */
#define BATERIA_MINIMA_MV        6000.0f

typedef struct {
    float x_mm, y_mm, ang_graus;
} posicao_tof_t;

static const posicao_tof_t POSICAO_TOF[4] = {
    {TOF_FE_X_MM, TOF_FE_Y_MM, TOF_FE_ANG},
    {TOF_FD_X_MM, TOF_FD_Y_MM, TOF_FD_ANG},
    {TOF_E_X_MM, TOF_E_Y_MM, TOF_E_ANG},
    {TOF_D_X_MM, TOF_D_Y_MM, TOF_D_ANG},
};

static struct {
    sim_labirinto_t lab;
    sim_pose_t pose;
    uint32_t tempo_ms;
    bool motores_habilitados;
    int16_t potencia[2];
    float velocidade_mm_s[2];
    float pulsos[2];
    double bateria_mv; /* double: a descarga de 1 ms some na precisão de um float */
    bool tof_ausente[4];
    uint16_t tof_ruido_mm;
    uint32_t tof_semente;
    bool tof_tem_pico[4];
    uint16_t tof_pico_mm[4];
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

static bool robo_encosta_em_parede(float x, float y)
{
    for (int i = 0; i < PONTOS_CONTORNO; i++) {
        float a = 2.0f * PI_F * i / PONTOS_CONTORNO;
        if (ponto_em_parede(x + ROBO_RAIO_MM * cosf(a), y + ROBO_RAIO_MM * sinf(a))) {
            return true;
        }
    }
    return false;
}

bool sim_mundo_iniciar(const char *labirinto_texto)
{
    memset(&m, 0, sizeof(m));
    if (!sim_labirinto_carregar(&m.lab, labirinto_texto)) {
        return false;
    }
    m.pose.x_mm = CELULA_MM / 2.0f;
    m.pose.y_mm = CELULA_MM / 2.0f;
    m.pose.theta_rad = PI_F / 2.0f;
    m.bateria_mv = BATERIA_CHEIA_MV;
    return true;
}

static void passo_1ms(void)
{
    const float dt = 0.001f;
    float deslocamento[2];

    for (int i = 0; i < 2; i++) {
        float alvo = m.motores_habilitados
                         ? VELOCIDADE_MAX_MM_S * m.potencia[i] / 1000.0f
                         : 0.0f;
        m.velocidade_mm_s[i] += (alvo - m.velocidade_mm_s[i]) * dt / CONSTANTE_TEMPO_MOTOR_S;
        deslocamento[i] = m.velocidade_mm_s[i] * dt;
    }

    /* Cinemática da tração diferencial: média das rodas avança, diferença gira. */
    float avanco = (deslocamento[0] + deslocamento[1]) / 2.0f;
    float giro = (deslocamento[1] - deslocamento[0]) / BITOLA_MM;
    float theta_medio = m.pose.theta_rad + giro / 2.0f;
    float novo_x = m.pose.x_mm + avanco * cosf(theta_medio);
    float novo_y = m.pose.y_mm + avanco * sinf(theta_medio);

    if (robo_encosta_em_parede(novo_x, novo_y)) {
        /* Bateu: o robô não sai do lugar e as rodas (e os encoders) param. */
        m.pose.colidiu = true;
        m.velocidade_mm_s[0] = m.velocidade_mm_s[1] = 0.0f;
    } else {
        m.pose.colidiu = false;
        m.pose.x_mm = novo_x;
        m.pose.y_mm = novo_y;
        m.pose.theta_rad += giro;
        m.pulsos[0] += deslocamento[0] / MM_POR_PULSO;
        m.pulsos[1] += deslocamento[1] / MM_POR_PULSO;
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

void sim_motor_habilitar(bool habilitar) { m.motores_habilitados = habilitar; }

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
    if (m.tof_ausente[tof]) {
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
            *distancia_mm = (uint16_t)aplicar_ruido(d);
            return true;
        }
    }
    *distancia_mm = SIM_TOF_FORA_DE_ALCANCE;
    return true;
}

uint16_t sim_bateria_adc_mv(void) { return (uint16_t)(m.bateria_mv / BATERIA_DIVISOR); }

bool sim_boot_pressionado(void) { return m.boot; }
void sim_led_definir(bool aceso) { m.led = aceso; }
void sim_buzzer_definir(bool ligado) { m.buzzer = ligado; }

void sim_definir_tof_ausente(int tof, bool ausente) { m.tof_ausente[tof] = ausente; }
void sim_definir_tof_ruido(uint16_t amplitude_mm, uint32_t semente)
{
    m.tof_ruido_mm = amplitude_mm;
    m.tof_semente = semente;
}

void sim_definir_tof_pico(int tof, uint16_t distancia_mm)
{
    m.tof_tem_pico[tof] = true;
    m.tof_pico_mm[tof] = distancia_mm;
}

void sim_definir_bateria_mv(uint16_t mv) { m.bateria_mv = mv; }
void sim_definir_boot(bool pressionado) { m.boot = pressionado; }

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
