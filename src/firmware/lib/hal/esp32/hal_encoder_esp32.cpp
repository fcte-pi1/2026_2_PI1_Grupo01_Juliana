#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Encoders pelo PCNT (FIRM-04, #130), quadratura x4: 7 PPR x 30 x 4 = 840 por volta.
 * O contador é de 16 bits e zera em ±LIMITE; a interrupção soma o LIMITE no acumulado.
 *
 * GPIO34, 35 e 39 não têm pull-up interno, precisa vir do encoder ou de fora.
 */

#include <Arduino.h>
#include <driver/pcnt.h>

#include "config/pinos.h"
#include "hal_encoder.h"

#define LIMITE 30000

/* ~3 µs, contra ruído das escovas */
#define FILTRO_CICLOS 250

typedef struct {
    pcnt_unit_t unidade;
    int8_t a;
    int8_t b;
    bool invertido; /* PROVISÓRIO: ver na bancada */
} encoder_pinos_t;

static const encoder_pinos_t ENCODERS[MOTOR_QTD] = {
    /* MOTOR_ESQ */ {PCNT_UNIT_0, PINO_ENC_E_A, PINO_ENC_E_B, false},
    /* MOTOR_DIR */ {PCNT_UNIT_1, PINO_ENC_D_A, PINO_ENC_D_B, false},
};

static portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED;
static volatile int32_t acumulado[MOTOR_QTD];
static int32_t ultimo[MOTOR_QTD];
static bool iniciado;

static void IRAM_ATTR ao_chegar_no_limite(void *arg)
{
    int motor = (int)(intptr_t)arg;
    uint32_t evento = 0;
    pcnt_get_event_status(ENCODERS[motor].unidade, &evento);

    portENTER_CRITICAL_ISR(&trava);
    if (evento & PCNT_EVT_H_LIM) {
        acumulado[motor] += LIMITE;
    } else if (evento & PCNT_EVT_L_LIM) {
        acumulado[motor] -= LIMITE;
    }
    portEXIT_CRITICAL_ISR(&trava);
}

static void configurar(int motor)
{
    const encoder_pinos_t *e = &ENCODERS[motor];

    /* canal 0: bordas de A, sentido por B */
    pcnt_config_t canal_a = {};
    canal_a.pulse_gpio_num = e->a;
    canal_a.ctrl_gpio_num = e->b;
    canal_a.channel = PCNT_CHANNEL_0;
    canal_a.unit = e->unidade;
    canal_a.pos_mode = PCNT_COUNT_DEC;
    canal_a.neg_mode = PCNT_COUNT_INC;
    canal_a.lctrl_mode = PCNT_MODE_REVERSE;
    canal_a.hctrl_mode = PCNT_MODE_KEEP;
    canal_a.counter_h_lim = LIMITE;
    canal_a.counter_l_lim = -LIMITE;
    pcnt_unit_config(&canal_a);

    /* canal 1: bordas de B, sentido por A */
    pcnt_config_t canal_b = canal_a;
    canal_b.pulse_gpio_num = e->b;
    canal_b.ctrl_gpio_num = e->a;
    canal_b.channel = PCNT_CHANNEL_1;
    canal_b.pos_mode = PCNT_COUNT_INC;
    canal_b.neg_mode = PCNT_COUNT_DEC;
    pcnt_unit_config(&canal_b);

    pcnt_set_filter_value(e->unidade, FILTRO_CICLOS);
    pcnt_filter_enable(e->unidade);

    pcnt_event_enable(e->unidade, PCNT_EVT_H_LIM);
    pcnt_event_enable(e->unidade, PCNT_EVT_L_LIM);
    pcnt_isr_handler_add(e->unidade, ao_chegar_no_limite, (void *)(intptr_t)motor);

    pcnt_counter_pause(e->unidade);
    pcnt_counter_clear(e->unidade);
    pcnt_counter_resume(e->unidade);
}

void hal_encoder_iniciar(void)
{
    if (iniciado) {
        return;
    }
    pcnt_isr_service_install(0);
    for (int i = 0; i < MOTOR_QTD; i++) {
        acumulado[i] = 0;
        ultimo[i] = 0;
        configurar(i);
    }
    iniciado = true;
}

int32_t hal_encoder_ler(motor_id_t motor)
{
    if (!iniciado || motor < 0 || motor >= MOTOR_QTD) {
        return 0;
    }

    int16_t contagem = 0;
    portENTER_CRITICAL(&trava);
    pcnt_get_counter_value(ENCODERS[motor].unidade, &contagem);
    int32_t total = acumulado[motor] + contagem;

    /*
     * O hardware zera o contador um pouco antes da interrupção somar o LIMITE.
     * Se a leitura cair nesse meio, o total pula ~LIMITE; desfaz o pulo.
     */
    int32_t salto = total - ultimo[motor];
    if (salto > LIMITE / 2) {
        total -= LIMITE;
    } else if (salto < -LIMITE / 2) {
        total += LIMITE;
    }
    ultimo[motor] = total;
    portEXIT_CRITICAL(&trava);

    return ENCODERS[motor].invertido ? -total : total;
}

#endif
