/*
 * Bancada do health-check com os drivers reais: ToF, DIP, motores, encoders e bateria.
 *
 * Não é o firmware do robô.
 *
 *   pio run -e esp32_hc_bancada -t upload --upload-port /dev/ttyACM0
 *   pio device monitor --port /dev/ttyACM0 --baud 115200
 *
 * Comandos (um caractere):
 *   x  liga o XSHUT de um sensor por vez e diz qual GPIO acorda um sensor no endereço 0x29
 *   c  troca o clock do I2C (50, 100 e 400 kHz, em rodízio); depois rode v ou h
 *   o  alterna o timeout das leituras do I2C entre 50 ms (padrão) e 500 ms
 *   f  descobre em quais GPIOs estão os XSHUT dos ToF e reinicia a placa (interruptor DESLIGADO)
 *   v  sobe só o sensor do GPIO5, roda a biblioteca com depuração e depois lê o ID do chip (só leitura)
 *   i  varre o I2C e lista os endereços que respondem (ToF iniciados: 0x30, 0x31, 0x32)
 *   t  lê os 3 ToF por ~2 s (mm); mão ou parede na frente e desconectar um sensor mostram o efeito
 *   d  lê o DIP agora; o programa também avisa sempre que uma via mudar
 *   b  tensão da bateria
 *   m  autoteste de motor e encoder: gira cada roda ~0,3 s a 30 %, RODAS NO AR,
 *      interruptor da bateria ligado
 *   h  health-check completo (inclui o giro das rodas), com o resultado item a item
 */

#include <Arduino.h>
#include <Adafruit_VL53L0X.h>
#include <Wire.h>

#include "bateria.h"
#include "config/robo.h"
#include "dip.h"
#include "hal_bateria.h"
#include "hal_dip.h"
#include "hal_encoder.h"
#include "hal_motor.h"
#include "hal_tof.h"
#include "health_check.h"
#include "telemetria.h"
#include "tof_enderecos.h"

static uint32_t clock_i2c_hz = TOF_I2C_HZ;
static uint16_t timeout_i2c_ms = 50;

static void mostrar_i2c_atual(void)
{
    Serial.printf("i2c: clock = %lu Hz, timeout de leitura = %u ms\n",
                  (unsigned long)Wire.getClock(), (unsigned)Wire.getTimeOut());
}

static void trocar_clock(void)
{
    clock_i2c_hz = clock_i2c_hz == 50000 ? 100000 : (clock_i2c_hz == 100000 ? 400000 : 50000);
    Wire.setClock(clock_i2c_hz);
    mostrar_i2c_atual();
}

static void alternar_timeout(void)
{
    timeout_i2c_ms = timeout_i2c_ms == 50 ? 500 : 50;
    Wire.setTimeOut(timeout_i2c_ms);
    mostrar_i2c_atual();
}

static bateria_t bateria;
static uint8_t dip_anterior = 0xFF;

static const char *nome_do_tipo(contrato_tipo_dip_t tipo)
{
    switch (tipo) {
    case CONTRATO_DIP_4X4: return "4x4";
    case CONTRATO_DIP_8X4: return "8x4";
    case CONTRATO_DIP_12X4: return "12x4";
    default: return "invalido";
    }
}

static const char *nome_do_componente(contrato_componente_t c)
{
    static const char *const NOMES[] = {"nenhum",         "bateria",        "tof_frontal",
                                        "tof_esquerdo",   "tof_direito",    "motor_esquerdo",
                                        "motor_direito",  "encoder_esquerdo", "encoder_direito"};
    return NOMES[c];
}

static void esperar(uint32_t ms) { delay(ms); }

static void varrer_i2c(void)
{
    Serial.println("i2c: enderecos que responderam:");
    int achados = 0;
    for (uint8_t endereco = 0x08; endereco < 0x78; endereco++) {
        Wire.beginTransmission(endereco);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  0x%02X\n", endereco);
            achados++;
        }
    }
    Serial.printf("i2c: %d dispositivo(s)\n", achados);
}

/* Só um XSHUT em nível alto por vez: mostra qual GPIO realmente acorda um sensor. */
static void testar_xshut(void)
{
    Serial.println("xshut: um GPIO por vez em nivel alto, sondando o endereco 0x29");
    for (int i = 0; i < TOF_QTD; i++) {
        for (int k = 0; k < TOF_QTD; k++) {
            digitalWrite(tof_pino_xshut((tof_id_t)k), LOW);
        }
        delay(TOF_ESPERA_XSHUT_MS);
        digitalWrite(tof_pino_xshut((tof_id_t)i), HIGH);
        delay(TOF_ESPERA_XSHUT_MS);
        Wire.beginTransmission(TOF_ENDERECO_PADRAO);
        bool acordou = Wire.endTransmission() == 0;
        Serial.printf("  %s (GPIO%d em alto): %s\n", hal_tof_nome((tof_id_t)i),
                      tof_pino_xshut((tof_id_t)i), acordou ? "sensor respondeu" : "nada respondeu");
    }
    Serial.println("xshut: reiniciando os ToF...");
    hal_tof_iniciar();
}

/*
 * GPIOs livres que podem receber um XSHUT. Ficam de fora: I2C (21, 22), serial (1, 3), BOOT (0),
 * LED (2), buzzer (12), STBY da ponte H (13), encoders e bateria (32, 34, 35, 36, 39) e a flash.
 * Os pinos de motor (14, 15, 18, 25, 26, 27) ficam sem efeito com o STBY baixo e o interruptor
 * desligado, que é como este teste deve rodar.
 */
static const uint8_t CANDIDATOS[] = {4, 5, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 33};
#define N_CANDIDATOS (sizeof(CANDIDATOS) / sizeof(CANDIDATOS[0]))

static bool sensor_acordado_em_0x29(void)
{
    Wire.beginTransmission(TOF_ENDERECO_PADRAO);
    return Wire.endTransmission() == 0;
}

static void descobrir_xshut(void)
{
    Serial.println("f: interruptor da bateria DESLIGADO; segurando todos os candidatos em nivel baixo...");
    for (size_t i = 0; i < N_CANDIDATOS; i++) {
        pinMode(CANDIDATOS[i], OUTPUT);
        digitalWrite(CANDIDATOS[i], LOW);
    }
    delay(50);

    if (sensor_acordado_em_0x29()) {
        Serial.println("f: ainda ha sensor acordado com todos os candidatos em baixo: o XSHUT dele nao esta");
        Serial.println("f: em nenhum desses GPIOs (ou esta solto/ligado ao 3V3). Nada a mapear.");
    } else {
        int achados = 0;
        for (size_t i = 0; i < N_CANDIDATOS; i++) {
            uint8_t pino = CANDIDATOS[i];
            digitalWrite(pino, HIGH);
            delay(TOF_ESPERA_XSHUT_MS);
            if (sensor_acordado_em_0x29()) {
                achados++;
                Serial.printf("f: GPIO%u acorda um sensor. Distancia por 3 s (passe a mao na frente de cada modulo):\n", pino);
                Adafruit_VL53L0X teste;
                bool ok = teste.begin(TOF_ENDERECO_PADRAO, false, &Wire,
                                      Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED);
                Serial.printf("   begin = %s (Status %d)\n", ok ? "OK" : "FALHOU", (int)teste.Status);
                for (int n = 0; ok && n < 30; n++) {
                    Serial.printf("   %u mm\n", teste.readRange());
                    delay(100);
                }
            }
            digitalWrite(pino, LOW);
            delay(TOF_ESPERA_XSHUT_MS);
        }
        Serial.printf("f: %d GPIO(s) acordam um sensor\n", achados);
    }
    Serial.println("f: reiniciando a placa para devolver os pinos aos drivers...");
    Serial.flush();
    ESP.restart();
}

/* Lê o ID do VL53L0X (registrador 0xC0, deve ser 0xEE). Só lê: nunca escreve em registradores. */
static void ler_id_do_chip(void)
{
    Wire.beginTransmission(TOF_ENDERECO_PADRAO);
    Wire.write((uint8_t)0xC0);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom((uint8_t)TOF_ENDERECO_PADRAO, (uint8_t)1) != 1) {
        Serial.println("v: nao consegui ler o ID do chip (0xC0)");
        return;
    }
    int id = Wire.read();
    Serial.printf("v: ID do chip (0xC0) = 0x%02X (VL53L0X = 0xEE)\n", id);
}

/* Sobe só o sensor do GPIO5 e deixa a biblioteca explicar por que falha. */
static void depurar_sensor(void)
{
    const tof_id_t id = TOF_DIREITO;
    mostrar_i2c_atual();
    Serial.printf("v: so %s (GPIO%d) em alto; a biblioteca vai imprimir o que encontrar\n",
                  hal_tof_nome(id), tof_pino_xshut(id));
    for (int k = 0; k < TOF_QTD; k++) {
        digitalWrite(tof_pino_xshut((tof_id_t)k), LOW);
    }
    delay(TOF_ESPERA_XSHUT_MS);
    digitalWrite(tof_pino_xshut(id), HIGH);
    delay(TOF_ESPERA_XSHUT_MS);

    Adafruit_VL53L0X teste;
    bool ok = teste.begin(tof_endereco(id), true, &Wire, Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED);
    Serial.printf("v: begin = %s, Status da biblioteca = %d (0 = sem erro)\n", ok ? "OK" : "FALHOU",
                  (int)teste.Status);
    ler_id_do_chip();
    Serial.println("v: reiniciando os ToF...");
    hal_tof_iniciar();
}

static void mostrar_tof(void)
{
    Serial.println("tof: frontal / esquerdo / direito em mm (-- = sem resposta)");
    for (int n = 0; n < 40; n++) {
        for (int i = 0; i < TOF_QTD; i++) {
            uint16_t mm = 0;
            if (hal_tof_ler_mm((tof_id_t)i, &mm)) {
                Serial.printf("%6u ", mm);
            } else {
                Serial.print("    -- ");
            }
        }
        Serial.println();
        delay(50);
    }
}

static void mostrar_dip(void)
{
    uint8_t vias = hal_dip_ler();
    Serial.printf("dip: vias 4321 = %c%c%c%c  codigo(1,2) = %u%u  tipo = %s\n",
                  (vias & 8) ? '1' : '0', (vias & 4) ? '1' : '0', (vias & 2) ? '1' : '0',
                  (vias & 1) ? '1' : '0', (vias >> 1) & 1, vias & 1,
                  nome_do_tipo(dip_tipo(vias)));
}

static void amostrar_bateria(void)
{
    for (int i = 0; i < BATERIA_MEDIA_AMOSTRAS; i++) {
        bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
        delay(2);
    }
    hal_motor_definir_tensao(bateria_ponte_mv(&bateria));
}

static void mostrar_bateria(void)
{
    amostrar_bateria();
    Serial.printf("bateria: fonte = %u mV  ponte = %u mV  nivel = %d (0 normal, 1 alerta, 2 baixa)\n",
                  bateria_mv(&bateria), bateria_ponte_mv(&bateria), (int)bateria_nivel(&bateria));
}

static void testar_motores(void)
{
    amostrar_bateria();
    Serial.println("motores: girando cada roda, rodas no ar...");
    int32_t pulsos[MOTOR_QTD];
    motor_autoteste_executar(esperar, pulsos);
    for (int m = 0; m < MOTOR_QTD; m++) {
        Serial.printf("  %s: %ld pulsos (minimo %d) -> %s\n", m == MOTOR_ESQ ? "esquerda" : "direita",
                      (long)pulsos[m], MOTOR_AUTOTESTE_MIN_PULSOS,
                      motor_autoteste_aprovado(pulsos[m]) ? "aprovada" : "REPROVADA");
    }
}

static void rodar_health_check(void)
{
    Serial.println("health-check: as rodas vao girar, rodas no ar...");
    uint32_t inicio = millis();
    hc_resultado_t r;
    hc_executar(&bateria, esperar, &r);
    uint32_t duracao = millis() - inicio;

    for (int i = 0; i < HC_ITENS; i++) {
        const contrato_hc_item_t *item = &r.item[i];
        if (item->tem_valor) {
            Serial.printf("  %-17s %s valor=%ld\n", nome_do_componente(item->componente),
                          item->aprovado ? "ok     " : "REPROVA", (long)item->valor);
        } else {
            Serial.printf("  %-17s %s valor=null\n", nome_do_componente(item->componente),
                          item->aprovado ? "ok     " : "REPROVA");
        }
    }
    Serial.printf("resultado: %s  tipo_dip=%s  primeiro reprovado=%s  duracao=%lu ms\n",
                  r.aprovado ? "APROVADO" : "REPROVADO", nome_do_tipo(r.tipo_dip),
                  nome_do_componente(r.reprovado), (unsigned long)duracao);
}

void setup()
{
    Serial.begin(115200);
    hal_motor_iniciar(); /* motores primeiro: parados e em standby */
    hal_encoder_iniciar();
    hal_bateria_iniciar();
    hal_dip_iniciar();
    bateria_iniciar(&bateria);
    bool tof_ok = hal_tof_iniciar();
    for (int i = 0; i < TOF_QTD; i++) {
        uint16_t mm = 0;
        Serial.printf("%s: %s\n", hal_tof_nome((tof_id_t)i),
                      hal_tof_ler_mm((tof_id_t)i, &mm) ? "respondeu" : "NAO respondeu");
    }
    Serial.printf("tof: %s\n", tof_ok ? "os 3 iniciaram" : "algum nao iniciou (confira XSHUT e I2C)");
    Serial.println("bancada do health-check: c o f v x i t d b m h (m e h giram as rodas: rodas no ar)");
}

void loop()
{
    if (Serial.available()) {
        switch (Serial.read()) {
        case 'c': trocar_clock(); break;
        case 'o': alternar_timeout(); break;
        case 'f': descobrir_xshut(); break;
        case 'v': depurar_sensor(); break;
        case 'x': testar_xshut(); break;
        case 'i': varrer_i2c(); break;
        case 't': mostrar_tof(); break;
        case 'd': mostrar_dip(); break;
        case 'b': mostrar_bateria(); break;
        case 'm': testar_motores(); break;
        case 'h': rodar_health_check(); break;
        default: break;
        }
    }

    uint8_t vias = hal_dip_ler();
    if (vias != dip_anterior) {
        dip_anterior = vias;
        mostrar_dip();
    }
    delay(50);
}
