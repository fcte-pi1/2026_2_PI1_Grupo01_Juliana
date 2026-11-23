#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * LED azul da placa (GPIO2) e buzzer (GPIO12). O buzzer é tratado como ativo: nível alto soa.
 * O GPIO12 é pino de strapping e precisa estar em nível baixo ao ligar, então o driver só o
 * configura depois do boot e o deixa em baixo.
 */

#include <Arduino.h>

#include "config/pinos.h"
#include "hal_led.h"

void hal_led_iniciar(void)
{
    pinMode(PINO_LED_STATUS, OUTPUT);
    digitalWrite(PINO_LED_STATUS, LOW);
    pinMode(PINO_BUZZER, OUTPUT);
    digitalWrite(PINO_BUZZER, LOW);
}

void hal_led_definir(bool aceso) { digitalWrite(PINO_LED_STATUS, aceso ? HIGH : LOW); }

void hal_buzzer_definir(bool ligado) { digitalWrite(PINO_BUZZER, ligado ? HIGH : LOW); }

#endif
