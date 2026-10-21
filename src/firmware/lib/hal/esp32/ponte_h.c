#include "ponte_h.h"

#include "hal_motor.h"

ponte_h_saida_t ponte_h_saida(int16_t potencia, bool invertido)
{
    ponte_h_saida_t saida = {0, false, false};

    if (potencia > MOTOR_POTENCIA_MAX) {
        potencia = MOTOR_POTENCIA_MAX;
    } else if (potencia < -MOTOR_POTENCIA_MAX) {
        potencia = -MOTOR_POTENCIA_MAX;
    }
    if (potencia == 0) {
        return saida;
    }

    bool frente = (potencia > 0) != invertido;
    uint32_t modulo = (uint32_t)(potencia > 0 ? potencia : -potencia);
    saida.duty = (uint16_t)((modulo * PONTE_H_DUTY_MAX + MOTOR_POTENCIA_MAX / 2) / MOTOR_POTENCIA_MAX);
    saida.in1 = frente;
    saida.in2 = !frente;
    return saida;
}
