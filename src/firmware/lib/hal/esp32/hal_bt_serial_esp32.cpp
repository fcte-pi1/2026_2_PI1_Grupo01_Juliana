#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Enlace com a ponte pela serial USB, no lugar do Bluetooth: a ponte do notebook abre
 * /dev/ttyACM0 e fala o mesmo contrato. O Bluetooth real (BluetoothSerial) substitui este
 * arquivo; a HAL e a telemetria não mudam.
 */

#include <Arduino.h>

#include "hal_bt.h"

bool hal_bt_iniciar(const char *nome_dispositivo)
{
    (void)nome_dispositivo;
    return true; /* quem chama já abriu a Serial */
}

bool hal_bt_conectado(void) { return true; }

size_t hal_bt_escrever(const char *dados, size_t tamanho)
{
    return Serial.write(reinterpret_cast<const uint8_t *>(dados), tamanho);
}

int hal_bt_ler_byte(void) { return Serial.read(); }

#endif
