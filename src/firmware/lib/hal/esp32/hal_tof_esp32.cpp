#if !defined(HAL_SIMULADA) && defined(ARDUINO)

/*
 * Os 3 VL53L0X na ESP32, pela biblioteca da Adafruit.
 *
 * Os sensores saem de fábrica no mesmo endereço (0x29). A inicialização segue a
 * sequência de lib/sensores/tof_enderecos: todos desligados pelo XSHUT, e um por
 * vez é ligado e recebe o endereço 0x30 + i (a Adafruit troca o endereço no begin).
 *
 * Cada sensor mede em modo contínuo, então hal_tof_ler_mm nunca espera o sensor:
 * devolve a última medida, e só falha se nenhuma chegou nos últimos
 * TOF_VALIDADE_MS ou se o I²C deu erro. Medida "sem nada à vista" sai como 8190,
 * que a lib sensores satura no alcance máximo.
 */

#include <Adafruit_VL53L0X.h>
#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "config/robo.h"
#include "hal_tof.h"
#include "tof_enderecos.h"

#define TOF_SEM_NADA_MM    8190
#define TOF_PERIODO_MS     40   /* maior que o tempo de medida (30 ms em modo rápido) */
#define TOF_VALIDADE_MS    200
#define TOF_TENTATIVAS     3
#define TOF_ESPERA_REINICIO_MS 20

static Adafruit_VL53L0X sensor[TOF_QTD];
static bool presente[TOF_QTD];
static uint16_t ultima_mm[TOF_QTD];
static uint32_t ultima_em_ms[TOF_QTD];

const char *hal_tof_nome(tof_id_t id)
{
    static const char *const NOMES[TOF_QTD] = {"tof_frontal", "tof_esquerdo", "tof_direito"};
    return (id >= 0 && id < TOF_QTD) ? NOMES[id] : "tof_desconhecido";
}

bool hal_tof_iniciar(void)
{
    Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
    Wire.setClock(TOF_I2C_HZ);

    for (int i = 0; i < TOF_QTD; i++) {
        presente[i] = false;
        ultima_em_ms[i] = 0;
    }

    tof_passo_t passos[TOF_PASSOS_INICIO];
    int n = tof_sequencia_inicio(passos);
    for (int p = 0; p < n; p++) {
        const tof_passo_t *passo = &passos[p];
        switch (passo->tipo) {
        case TOF_PASSO_XSHUT_BAIXO:
            pinMode(passo->pino, OUTPUT);
            digitalWrite(passo->pino, LOW);
            break;
        case TOF_PASSO_XSHUT_ALTO:
            digitalWrite(passo->pino, HIGH);
            break;
        case TOF_PASSO_ESPERAR_MS:
            delay(passo->valor);
            break;
        case TOF_PASSO_TROCAR_ENDERECO: {
            int i = passo->sensor;
            /*
             * A biblioteca fica tentando ler um sensor que não responde, então só chama begin
             * se houver ACK. Alguns módulos falham no primeiro begin: reinicia o sensor e tenta
             * de novo.
             */
            for (int tentativa = 0; tentativa < TOF_TENTATIVAS && !presente[i]; tentativa++) {
                if (tentativa > 0) {
                    digitalWrite(passo->pino, LOW);
                    delay(TOF_ESPERA_REINICIO_MS);
                    digitalWrite(passo->pino, HIGH);
                    delay(TOF_ESPERA_REINICIO_MS);
                }
                Wire.beginTransmission(TOF_ENDERECO_PADRAO);
                if (Wire.endTransmission() != 0) {
                    continue;
                }
                presente[i] = sensor[i].begin(passo->valor, false, &Wire,
                                              Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED) &&
                              sensor[i].startRangeContinuous(TOF_PERIODO_MS);
            }
            break;
        }
        }
    }

    bool todos = true;
    for (int i = 0; i < TOF_QTD; i++) {
        todos = todos && presente[i];
    }
    return todos;
}

bool hal_tof_ler_mm(tof_id_t id, uint16_t *distancia_mm)
{
    if (id < 0 || id >= TOF_QTD || !presente[id]) {
        return false;
    }

    if (sensor[id].isRangeComplete()) {
        uint16_t mm = sensor[id].readRangeResult();
        if (sensor[id].Status != VL53L0X_ERROR_NONE) {
            return false; /* erro de I²C: não usa o dado e não renova a validade */
        }
        ultima_mm[id] = mm == 0xFFFF ? TOF_SEM_NADA_MM : mm;
        ultima_em_ms[id] = millis();
    }

    if (millis() - ultima_em_ms[id] > TOF_VALIDADE_MS) {
        return false;
    }
    *distancia_mm = ultima_mm[id];
    return true;
}

#endif
