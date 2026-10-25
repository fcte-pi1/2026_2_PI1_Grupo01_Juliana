/*
 * Bancada dos drivers reais (lib/hal/esp32): motores, encoders, bateria e NVS.
 *
 * Não é o firmware do robô. Serve para o CI compilar os drivers sem a HAL
 * simulada e para conferir motor e encoder na bancada antes do FIRM-10.
 *
 *   pio run -e esp32_drivers -t upload
 *   pio device monitor
 *
 * Os motores só giram por comando, a 30 %. Rodas no ar.
 *   e / E  esquerdo frente / ré      d / D  direito frente / ré
 *   p      para os dois              n      grava e lê um contador na NVS
 */

#include <Arduino.h>

#include "bateria.h"
#include "hal_bateria.h"
#include "hal_encoder.h"
#include "hal_motor.h"
#include "hal_nvs.h"

#define POTENCIA_TESTE 300
#define PERIODO_MS     200

static bateria_t bateria;

static void girar(motor_id_t motor, int16_t potencia)
{
    hal_motor_definir(MOTOR_ESQ, motor == MOTOR_ESQ ? potencia : 0);
    hal_motor_definir(MOTOR_DIR, motor == MOTOR_DIR ? potencia : 0);
    hal_motor_habilitar(true);
}

static void parar(void)
{
    hal_motor_definir(MOTOR_ESQ, 0);
    hal_motor_definir(MOTOR_DIR, 0);
    hal_motor_habilitar(false);
}

static void testar_nvs(void)
{
    uint32_t contador = 0;
    hal_nvs_ler("bancada", &contador, sizeof(contador));
    contador++;
    bool ok = hal_nvs_gravar("bancada", &contador, sizeof(contador));
    Serial.printf("nvs: contador=%lu %s\n", (unsigned long)contador, ok ? "ok" : "ERRO");
}

void setup()
{
    Serial.begin(115200);
    hal_motor_iniciar();
    hal_encoder_iniciar();
    hal_bateria_iniciar();
    hal_nvs_iniciar();
    bateria_iniciar(&bateria);
    Serial.println("bancada dos drivers: e E d D p n");
}

void loop()
{
    if (Serial.available()) {
        switch (Serial.read()) {
        case 'e': girar(MOTOR_ESQ, POTENCIA_TESTE); break;
        case 'E': girar(MOTOR_ESQ, -POTENCIA_TESTE); break;
        case 'd': girar(MOTOR_DIR, POTENCIA_TESTE); break;
        case 'D': girar(MOTOR_DIR, -POTENCIA_TESTE); break;
        case 'p': parar(); break;
        case 'n': testar_nvs(); break;
        default: break;
        }
    }

    bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
    if (bateria_pronta(&bateria)) {
        hal_motor_definir_tensao(bateria_ponte_mv(&bateria));
    }
    Serial.printf("enc_e=%ld enc_d=%ld bat=%u mV ponte=%u mV\n",
                  (long)hal_encoder_ler(MOTOR_ESQ), (long)hal_encoder_ler(MOTOR_DIR),
                  bateria_mv(&bateria), bateria_ponte_mv(&bateria));
    delay(PERIODO_MS);
}
