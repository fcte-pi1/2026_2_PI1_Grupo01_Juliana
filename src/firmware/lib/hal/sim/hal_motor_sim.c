#ifdef HAL_SIMULADA

#include "hal_encoder.h"
#include "hal_motor.h"
#include "sim_mundo.h"

void hal_motor_iniciar(void)
{
    /* Mesmo comportamento esperado do driver real: começa em standby e parado. */
    sim_motor_habilitar(false);
    sim_motor_definir(MOTOR_ESQ, 0);
    sim_motor_definir(MOTOR_DIR, 0);
}

void hal_motor_habilitar(bool habilitar) { sim_motor_habilitar(habilitar); }

void hal_motor_definir(motor_id_t motor, int16_t potencia)
{
    if (motor >= 0 && motor < MOTOR_QTD) {
        sim_motor_definir(motor, potencia);
    }
}

void hal_encoder_iniciar(void) {}

int32_t hal_encoder_ler(motor_id_t motor)
{
    return (motor >= 0 && motor < MOTOR_QTD) ? sim_encoder(motor) : 0;
}

#endif /* HAL_SIMULADA */
