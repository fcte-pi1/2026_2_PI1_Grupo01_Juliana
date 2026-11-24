#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Encoders pelo PCNT (FIRM-04, #130), quadratura x4: 7 PPR x 30 x 4 = 840 por volta.
 * O contador é de 16 bits e zera em ±LIMITE; a leitura (200 Hz no controle) percebe o salto e soma o
 * LIMITE ao acumulado (encoder_soma), sem interrupção.
 *
 * GPIO34, 35 e 39 não têm pull-up interno, precisa vir do encoder ou de fora.
 */

#include <Arduino.h>
#include <driver/pcnt.h>

#include "config/pinos.h"
#include "encoder_soma.h"
#include "hal_encoder.h"

#define LIMITE 30000

/* ~3 µs, contra ruído das escovas */
#define FILTRO_CICLOS 250

typedef struct {
    pcnt_unit_t unidade;
    int8_t a;
    int8_t b;
    bool invertido; /* acompanha o motor: a roda para a frente conta positivo */
} encoder_pinos_t;

static const encoder_pinos_t ENCODERS[MOTOR_QTD] = {
    /* MOTOR_ESQ */ {PCNT_UNIT_0, PINO_ENC_E_A, PINO_ENC_E_B, true},
    /* MOTOR_DIR */ {PCNT_UNIT_1, PINO_ENC_D_A, PINO_ENC_D_B, false},
};

static portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED;
static encoder_soma_t soma[MOTOR_QTD];
static bool iniciado;

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

    pcnt_counter_pause(e->unidade);
    pcnt_counter_clear(e->unidade);
    pcnt_counter_resume(e->unidade);
    encoder_soma_iniciar(&soma[motor], 0);
}

void hal_encoder_iniciar(void)
{
    if (iniciado) {
        return;
    }
    for (int i = 0; i < MOTOR_QTD; i++) {
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
    int32_t total = encoder_soma_atualizar(&soma[motor], contagem, LIMITE);
    portEXIT_CRITICAL(&trava);

    return ENCODERS[motor].invertido ? -total : total;
}

#endif
