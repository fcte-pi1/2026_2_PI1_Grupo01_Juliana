#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Motores na TB6612 (FIRM-04, #130): PWM pelo LEDC e sentido em IN1/IN2.
 * A conta de duty e sentido está em ponte_h.c.
 *
 * STBY vai para baixo antes de tudo. Antes do hal_motor_iniciar quem segura a
 * ponte desligada é o resistor de MOT_EN (ver config/pinos.h).
 */

#include <Arduino.h>

#include "config/pinos.h"
#include "hal_motor.h"
#include "ponte_h.h"

#define CANAL_PWM_E 0
#define CANAL_PWM_D 1

typedef struct {
    int8_t pwm;
    int8_t in1;
    int8_t in2;
    uint8_t canal;
    bool invertido; /* PROVISÓRIO: ver na bancada */
} motor_pinos_t;

static const motor_pinos_t MOTORES[MOTOR_QTD] = {
    /* MOTOR_ESQ */ {PINO_MOT_E_PWM, PINO_MOT_E_IN1, PINO_MOT_E_IN2, CANAL_PWM_E, false},
    /* MOTOR_DIR */ {PINO_MOT_D_PWM, PINO_MOT_D_IN1, PINO_MOT_D_IN2, CANAL_PWM_D, false},
};

static bool iniciado;

/* -1 = pino ainda sem GPIO (PINO_MOT_D_IN2) */
static void saida(int8_t pino, bool nivel)
{
    if (pino < 0) {
        return;
    }
    digitalWrite(pino, nivel ? HIGH : LOW); /* antes do pinMode para não dar pulso */
    pinMode(pino, OUTPUT);
}

static void aplicar(motor_id_t motor, int16_t potencia)
{
    const motor_pinos_t *m = &MOTORES[motor];
    ponte_h_saida_t s = ponte_h_saida(potencia, m->invertido);
    if (m->in1 >= 0) {
        digitalWrite(m->in1, s.in1 ? HIGH : LOW);
    }
    if (m->in2 >= 0) {
        digitalWrite(m->in2, s.in2 ? HIGH : LOW);
    }
    ledcWrite(m->canal, s.duty);
}

void hal_motor_iniciar(void)
{
    saida(PINO_MOT_STBY, false);

    for (int i = 0; i < MOTOR_QTD; i++) {
        const motor_pinos_t *m = &MOTORES[i];
        ponte_h_saida_t parado = ponte_h_saida(0, m->invertido);
        saida(m->in1, parado.in1);
        saida(m->in2, parado.in2);
        ledcSetup(m->canal, PONTE_H_FREQUENCIA_HZ, PONTE_H_RESOLUCAO_BITS);
        ledcWrite(m->canal, parado.duty);
        ledcAttachPin(m->pwm, m->canal);
    }
    iniciado = true;
}

void hal_motor_habilitar(bool habilitar)
{
    if (iniciado) {
        digitalWrite(PINO_MOT_STBY, habilitar ? HIGH : LOW);
    }
}

void hal_motor_definir(motor_id_t motor, int16_t potencia)
{
    if (iniciado && motor >= 0 && motor < MOTOR_QTD) {
        aplicar(motor, potencia);
    }
}

#endif
