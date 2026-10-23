#include "ponte_h.h"

#include "config/robo.h"
#include "hal_motor.h"

ponte_h_saida_t ponte_h_saida(int16_t potencia, bool invertido, uint16_t ponte_mv)
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
    if (ponte_mv < MOTOR_TENSAO_NOMINAL_MV) {
        ponte_mv = MOTOR_TENSAO_NOMINAL_MV;
    }
    /* 64 bits: 1000 x 1023 x 6000 não cabe em 32 */
    uint64_t divisor = (uint64_t)MOTOR_POTENCIA_MAX * ponte_mv;
    uint64_t duty = ((uint64_t)modulo * PONTE_H_DUTY_MAX * MOTOR_TENSAO_NOMINAL_MV + divisor / 2) / divisor;
    saida.duty = (uint16_t)duty;
    saida.in1 = frente;
    saida.in2 = !frente;
    return saida;
}
