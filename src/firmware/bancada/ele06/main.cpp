/*
 * ELE-06 — Teste de bancada: configuração do microcontrolador, watchdog e I²C
 * Projeto Ratatouille · PI1 2026.2 · Eletrônica / Dupla 3
 *
 * Não é o firmware do robô. É um programa à parte, só para a bancada, que usa
 * os MESMOS pinos do firmware (config/pinos.h) e prova que:
 *   T1  a ESP32 sobe com todas as saídas de motor em 0 (ponte H desligada);
 *   T2  as linhas SDA/SCL do I²C estão livres (nível alto em repouso);
 *   T3  os 3 VL53L0X recebem endereços próprios pelo XSHUT (0x30, 0x31, 0x32);
 *   T4  o barramento aguenta leituras seguidas a 400 kHz sem erro;
 *   WDT o watchdog reinicia a placa e corta os motores se uma tarefa travar.
 *
 * Gravar e abrir o monitor (dentro de src/firmware):
 *   pio run -e ele06_bancada -t upload
 *   pio device monitor
 *
 * Comandos no monitor serial (115200):
 *   h ajuda | r refazer testes | t travar a tarefa de controle (teste do watchdog)
 *   s status | b bipe
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <soc/gpio_reg.h>

#include "config/pinos.h"
#include "config/robo.h"

/* ------------------------------------------------------------ parâmetros */

#define WDT_TIMEOUT_S       1        /* tarefa parada por 1 s -> pânico -> reset */
#define I2C_FREQ_HZ         400000
#define I2C_TIMEOUT_MS      20

#define VL53_END_PADRAO     0x29     /* todo VL53L0X sai de fábrica neste endereço */
#define VL53_END_BASE       0x30     /* sensor i fica em 0x30 + i (igual ao FIRM-03) */
#define VL53_REG_ID         0xC0     /* registrador de identificação */
#define VL53_ID_ESPERADO    0xEE
#define VL53_REG_ENDERECO   0x8A     /* escrever aqui troca o endereço I²C */
#define VL53_ESPERA_MS      10       /* o sensor leva ~1,2 ms para acordar */

#define ESTRESSE_LEITURAS   250      /* por sensor */

/* ------------------------------------------------------------ estado */

struct SensorTof {
    const char *nome;
    int8_t xshut;
    uint8_t endereco;
    bool respondeu_padrao;
    bool id_ok;
    bool endereco_ok;
    uint32_t erros;
};

/* Mesma ordem de lib/sensores/tof_enderecos.c: frontal, esquerdo, direito */
static SensorTof sensores[] = {
    {"frontal",  PINO_XSHUT_F, VL53_END_BASE + 0, false, false, false, 0},
    {"esquerdo", PINO_XSHUT_E, VL53_END_BASE + 1, false, false, false, 0},
    {"direito",  PINO_XSHUT_D, VL53_END_BASE + 2, false, false, false, 0},
};
static const int QTD_SENSORES = sizeof(sensores) / sizeof(sensores[0]);

/* Pinos de motor como máscara de bits (todos < 32), para desligar dentro da ISR */
static uint32_t mascara_motores = 0;

/* Sobrevivem a reset por watchdog/pânico (só zeram quando a placa é desligada) */
static RTC_NOINIT_ATTR uint32_t resets_anormais;
static RTC_NOINIT_ATTR uint32_t marca_rtc;
#define MARCA_VALIDA 0x52415441u /* "RATA" */

static volatile bool travar_controle = false;
static volatile uint32_t ciclos_controle = 0;
static bool reset_anormal = false;
static bool barramento_ok = false;

/* ------------------------------------------------------------ pinos */

static void saida_em_zero(int8_t pino)
{
    if (pino < 0) return;            /* -1 = pino ainda não atribuído no pinos.h */
    digitalWrite(pino, LOW);         /* nível antes da direção: sem pulso no boot */
    pinMode(pino, OUTPUT);
}

static void acrescentar_motor(int8_t pino)
{
    if (pino >= 0 && pino < 32) mascara_motores |= (1UL << pino);
}

static void pinos_estado_seguro(void)
{
    const int8_t motores[] = {PINO_MOT_STBY, PINO_MOT_E_PWM, PINO_MOT_E_IN1, PINO_MOT_E_IN2,
                              PINO_MOT_D_PWM, PINO_MOT_D_IN1, PINO_MOT_D_IN2};
    for (int8_t p : motores) {
        saida_em_zero(p);
        acrescentar_motor(p);
    }
    for (const SensorTof &s : sensores) saida_em_zero(s.xshut);   /* ToF desligados */
    saida_em_zero(PINO_BUZZER);
    saida_em_zero(PINO_LED_STATUS);

    const int8_t dips[] = {PINO_DIP_1, PINO_DIP_2, PINO_DIP_3, PINO_DIP_4};
    for (int8_t p : dips) pinMode(p, INPUT_PULLUP);
}

/* Chamada pelo próprio ESP-IDF dentro da interrupção do watchdog, antes do
 * pânico: zera STBY, PWM e IN de uma vez, escrevendo direto no registrador. */
extern "C" void IRAM_ATTR esp_task_wdt_isr_user_handler(void)
{
    REG_WRITE(GPIO_OUT_W1TC_REG, mascara_motores);
}

/* ------------------------------------------------------------ watchdog */

static const char *nome_reset(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:  return "POWERON (ligou a placa)";
    case ESP_RST_EXT:      return "EXT (pino EN)";
    case ESP_RST_SW:       return "SW (reinicio por software)";
    case ESP_RST_PANIC:    return "PANIC (erro de software)";
    case ESP_RST_INT_WDT:  return "INT_WDT (interrupcao travada)";
    case ESP_RST_TASK_WDT: return "TASK_WDT (tarefa travada)";
    case ESP_RST_WDT:      return "WDT (outro watchdog)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (queda de tensao)";
    default:               return "OUTRO";
    }
}

static void relatorio_reset(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    if (marca_rtc != MARCA_VALIDA || r == ESP_RST_POWERON) {
        marca_rtc = MARCA_VALIDA;
        resets_anormais = 0;
    }
    reset_anormal = (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
                     r == ESP_RST_WDT || r == ESP_RST_BROWNOUT);
    if (reset_anormal) resets_anormais++;
    Serial.printf("Ultimo reset: %s | resets anormais desde que ligou: %lu\n", nome_reset(r),
                  (unsigned long)resets_anormais);
}

/* ------------------------------------------------------------ I²C */

static bool i2c_responde(uint8_t endereco)
{
    Wire.beginTransmission(endereco);
    return Wire.endTransmission() == 0;
}

static bool i2c_ler_reg(uint8_t endereco, uint8_t reg, uint8_t *valor)
{
    Wire.beginTransmission(endereco);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;   /* sem STOP: repeated start */
    if (Wire.requestFrom((int)endereco, 1) != 1) return false;
    *valor = Wire.read();
    return true;
}

static bool i2c_escrever_reg(uint8_t endereco, uint8_t reg, uint8_t valor)
{
    Wire.beginTransmission(endereco);
    Wire.write(reg);
    Wire.write(valor);
    return Wire.endTransmission() == 0;
}

static int i2c_varredura(const char *titulo)
{
    int n = 0;
    Serial.printf("  %s: ", titulo);
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (i2c_responde(a)) {
            Serial.printf("0x%02X ", a);
            n++;
        }
    }
    Serial.println(n ? "" : "(nenhum)");
    return n;
}

/* T2: antes de ligar o I²C, as linhas devem estar em 1 (pull-ups dos módulos) */
static bool i2c_linhas_livres(void)
{
    pinMode(PINO_I2C_SDA, INPUT_PULLDOWN);
    pinMode(PINO_I2C_SCL, INPUT_PULLDOWN);
    delay(2);
    int sda = digitalRead(PINO_I2C_SDA);
    int scl = digitalRead(PINO_I2C_SCL);
    Serial.printf("  Linhas em repouso: SDA=%d SCL=%d\n", sda, scl);
    if (!sda || !scl) {
        Serial.println("  -> linha em 0: nenhum modulo ligado, falta pull-up ou curto para GND");
    }
    return sda && scl;
}

/* T3: liga um sensor por vez e troca o endereço de cada um */
static int tof_enderecar(void)
{
    int ok = 0;
    for (SensorTof &s : sensores) {
        if (s.xshut >= 0) digitalWrite(s.xshut, LOW);
    }
    delay(VL53_ESPERA_MS);
    if (i2c_varredura("Todos os XSHUT em 0 (esperado: nenhum)") > 0) {
        Serial.println("  -> algo respondeu com tudo desligado: XSHUT solto ou outro dispositivo");
    }

    for (SensorTof &s : sensores) {
        s.respondeu_padrao = s.id_ok = s.endereco_ok = false;
        if (s.xshut < 0) continue;

        digitalWrite(s.xshut, HIGH);
        delay(VL53_ESPERA_MS);

        s.respondeu_padrao = i2c_responde(VL53_END_PADRAO);
        if (s.respondeu_padrao) {
            uint8_t id = 0;
            s.id_ok = i2c_ler_reg(VL53_END_PADRAO, VL53_REG_ID, &id) && id == VL53_ID_ESPERADO;
            if (!s.id_ok) Serial.printf("  -> %s: ID 0x%02X (esperado 0xEE)\n", s.nome, id);
            i2c_escrever_reg(VL53_END_PADRAO, VL53_REG_ENDERECO, s.endereco & 0x7F);
            delay(2);
            s.endereco_ok = i2c_responde(s.endereco);
        }
        if (s.id_ok && s.endereco_ok) ok++;
        Serial.printf("  %-9s XSHUT=GPIO%-2d  0x29:%s  ID:%s  -> 0x%02X:%s\n", s.nome, s.xshut,
                      s.respondeu_padrao ? "ok" : "--", s.id_ok ? "ok" : "--", s.endereco,
                      s.endereco_ok ? "ok" : "FALHA");
    }
    i2c_varredura("Depois do enderecamento (esperado: 0x30 0x31 0x32)");
    return ok;
}

/* T4: lê o ID de cada sensor muitas vezes e conta os erros */
static uint32_t i2c_estresse(void)
{
    uint32_t total = 0, leituras = 0;
    uint32_t t0 = micros();
    for (SensorTof &s : sensores) {
        s.erros = 0;
        if (!s.endereco_ok) continue;
        for (int k = 0; k < ESTRESSE_LEITURAS; k++) {
            uint8_t id = 0;
            if (!i2c_ler_reg(s.endereco, VL53_REG_ID, &id) || id != VL53_ID_ESPERADO) s.erros++;
            leituras++;
        }
        total += s.erros;
    }
    uint32_t dt = micros() - t0;
    if (leituras) {
        Serial.printf("  %lu leituras em %lu ms (%lu us cada), %lu erros\n",
                      (unsigned long)leituras, (unsigned long)(dt / 1000),
                      (unsigned long)(dt / leituras), (unsigned long)total);
    }
    return total;
}

/* ------------------------------------------------------------ apoio */

static float bateria_volts(void)
{
    uint32_t soma = 0;
    for (int i = 0; i < 16; i++) soma += analogReadMilliVolts(PINO_VBAT_SENSE);
    return (soma / 16.0f) / 1000.0f * BATERIA_DIVISOR;
}

static void bipe(int ms)
{
    digitalWrite(PINO_BUZZER, HIGH);
    delay(ms);
    digitalWrite(PINO_BUZZER, LOW);
}

static const char *pf(bool ok) { return ok ? "PASSOU" : "FALHOU"; }

static void bateria_de_testes(void)
{
    Serial.println("\n----------------------- TESTES -----------------------");

    /* T1: lê o registrador de saída: todos os pinos de motor e o buzzer em 0 */
    uint32_t mascara_t1 = mascara_motores | (1UL << PINO_BUZZER);
    bool t1 = (REG_READ(GPIO_OUT_REG) & mascara_t1) == 0;

    Serial.println("T2/T3 - barramento I2C");
    bool t2 = barramento_ok;
    int tof_ok = tof_enderecar();
    bool t3 = (tof_ok == QTD_SENSORES);

    Serial.println("T4 - estresse do barramento");
    uint32_t erros = tof_ok ? i2c_estresse() : 1;
    bool t4 = tof_ok > 0 && erros == 0;

    float vb = bateria_volts();

    Serial.println("\n==================== RESUMO ELE-06 ====================");
    Serial.printf(" T1 Saidas de motor em 0 (ponte desligada)  %s\n", pf(t1));
    Serial.printf(" T2 Linhas I2C livres (SDA/SCL = 1)         %s\n", pf(t2));
    Serial.printf(" T3 Enderecamento dos ToF (%d/%d)             %s\n", tof_ok, QTD_SENSORES, pf(t3));
    for (const SensorTof &s : sensores) {
        Serial.printf("      %-9s 0x%02X  %s\n", s.nome, s.endereco, s.endereco_ok ? "ok" : "FALHA");
    }
    Serial.printf(" T4 Estresse I2C 400 kHz (%lu erros)          %s\n", (unsigned long)erros, pf(t4));
    Serial.printf(" Watchdog: %d s | ultimo reset anormal? %s | resets anormais: %lu\n", WDT_TIMEOUT_S,
                  reset_anormal ? "SIM" : "nao", (unsigned long)resets_anormais);
    Serial.printf(" Bateria (informativo): %.2f V%s\n", vb, vb < 1.0f ? "  (so USB)" : "");
    if (PINO_MOT_D_IN2 < 0) Serial.println(" Aviso: PINO_MOT_D_IN2 = -1 no pinos.h (sem pino atribuido)");
    Serial.println("=======================================================");
    Serial.println("Comandos: h ajuda | r testes | t travar (watchdog) | s status | b bipe\n");

    bipe((t1 && t2 && t3 && t4) ? 80 : 600);
}

/* ------------------------------------------------------------ tarefa vigiada */

/* No firmware real, aqui ficaria o laço de navegação/motores (núcleo 1). */
static void tarefa_controle(void *)
{
    esp_task_wdt_add(NULL);                       /* passa a ser vigiada pelo watchdog */
    bool led = false;
    TickType_t ultimo = xTaskGetTickCount();
    for (;;) {
        if (travar_controle) {
            /* Bug simulado: laço infinito sem alimentar o watchdog.
             * Esperado: em ~1 s os motores são zerados, o pânico aparece e a placa reinicia. */
            Serial.println("Tarefa de controle TRAVADA de proposito. Aguardando o watchdog...");
            uint32_t t0 = millis();
            for (;;) {
                if (millis() - t0 > 5000) {
                    Serial.println("O watchdog NAO disparou em 5 s!");
                    t0 = millis();
                }
            }
        }
        if (++ciclos_controle % 50 == 0) {        /* LED a 1 Hz = controle vivo */
            led = !led;
            digitalWrite(PINO_LED_STATUS, led);
        }
        esp_task_wdt_reset();                     /* "alimenta" o watchdog */
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(10));
    }
}

/* ------------------------------------------------------------ Arduino */

void setup()
{
    pinos_estado_seguro();                        /* SEMPRE antes de qualquer outra coisa */
    Serial.begin(115200);
    delay(200);

    Serial.println("\n=== Ratatouille - ELE-06 (teste de bancada) ===");
    relatorio_reset();
    Serial.printf("Chip %s rev %d, %d nucleos, %lu MHz, flash %lu MB\n", ESP.getChipModel(),
                  ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz(),
                  (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
    Serial.printf("DIP (0 = chave ligada): 1=%d 2=%d 3=%d 4=%d\n", digitalRead(PINO_DIP_1),
                  digitalRead(PINO_DIP_2), digitalRead(PINO_DIP_3), digitalRead(PINO_DIP_4));

    analogSetPinAttenuation(PINO_VBAT_SENSE, ADC_11db);

    barramento_ok = i2c_linhas_livres();
    Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL, I2C_FREQ_HZ);
    Wire.setTimeOut(I2C_TIMEOUT_MS);

    /* O core Arduino já liga o watchdog com 5 s; aqui ele passa a 1 s, com pânico */
    esp_task_wdt_init(WDT_TIMEOUT_S, true);

    bateria_de_testes();

    xTaskCreatePinnedToCore(tarefa_controle, "controle", 4096, NULL, 10, NULL, 1);
}

void loop()
{
    if (!Serial.available()) {
        delay(20);
        return;
    }
    switch (Serial.read()) {
    case 'r': bateria_de_testes(); break;
    case 't': travar_controle = true; break;
    case 'b': bipe(150); break;
    case 's':
        Serial.printf("bateria=%.2f V  ciclos do controle=%lu  saidas de motor=%s\n", bateria_volts(),
                      (unsigned long)ciclos_controle,
                      (REG_READ(GPIO_OUT_REG) & mascara_motores) ? "ALGUMA EM 1" : "todas em 0");
        break;
    case 'h':
        Serial.println("r = refazer os testes\n"
                       "t = travar a tarefa de controle: a placa deve reiniciar em ~1 s\n"
                       "    e mostrar 'Ultimo reset: TASK_WDT'\n"
                       "s = status | b = bipe");
        break;
    default: break;
    }
}
