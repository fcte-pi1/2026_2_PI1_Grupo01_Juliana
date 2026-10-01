#include "movimento.h"

#include "hal_encoder.h"
#include "hal_motor.h"

void mov_iniciar(void)
{
    hal_motor_iniciar();
    hal_encoder_iniciar();
}

void mov_parar(void)
{
    hal_motor_definir(MOTOR_ESQ, 0);
    hal_motor_definir(MOTOR_DIR, 0);
}
