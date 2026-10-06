/*
 * Firmware do Micromouse Ratatouille — ponto de entrada.
 *
 * O Arduino chama setup() uma vez; depois disso o trabalho fica com as
 * tarefas do FreeRTOS criadas em app_iniciar(), e o loop() não faz nada.
 */

#include <Arduino.h>

#include "app/app.h"

void setup()
{
    Serial.begin(115200);
    app_iniciar();
}

void loop()
{
    vTaskDelay(portMAX_DELAY);
}
