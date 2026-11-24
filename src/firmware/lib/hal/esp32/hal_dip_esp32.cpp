#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * DIP switch na ESP32: pull-up interno em GPIO23, 17, 16 e 19.
 * Chave ligada = pino no terra = LOW, então o nível é invertido aqui.
 */

#include <Arduino.h>

#include "config/pinos.h"
#include "hal_dip.h"

static const uint8_t PINOS_DIP[4] = {PINO_DIP_1, PINO_DIP_2, PINO_DIP_3, PINO_DIP_4};

void hal_dip_iniciar(void)
{
    for (uint8_t pino : PINOS_DIP) {
        pinMode(pino, INPUT_PULLUP);
    }
}

uint8_t hal_dip_ler(void)
{
    uint8_t vias = 0;
    for (int i = 0; i < 4; i++) {
        if (digitalRead(PINOS_DIP[i]) == LOW) {
            vias = (uint8_t)(vias | (1u << i));
        }
    }
    return vias;
}

#endif
