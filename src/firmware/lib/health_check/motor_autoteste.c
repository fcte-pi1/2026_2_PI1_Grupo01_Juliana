#include "motor_autoteste.h"

#include "config/robo.h"
#include "hal_encoder.h"

bool motor_autoteste_aprovado(int32_t pulsos)
{
    return pulsos >= MOTOR_AUTOTESTE_MIN_PULSOS;
}

void motor_autoteste_executar(hc_esperar_t esperar, int32_t pulsos[MOTOR_QTD])
{
    for (int m = 0; m < MOTOR_QTD; m++) {
        motor_id_t motor = (motor_id_t)m;
        int32_t antes = hal_encoder_ler(motor);

        hal_motor_habilitar(true);
        hal_motor_definir(motor, MOTOR_AUTOTESTE_POTENCIA);
        esperar(MOTOR_AUTOTESTE_MS);
        hal_motor_definir(motor, 0);
        hal_motor_habilitar(false);

        int32_t contados = hal_encoder_ler(motor) - antes;
        pulsos[m] = contados < 0 ? -contados : contados;
        esperar(MOTOR_AUTOTESTE_MS / 3);
    }
}
