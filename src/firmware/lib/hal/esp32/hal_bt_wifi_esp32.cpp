#if !defined(HAL_SIMULADA) && defined(ARDUINO) && defined(WIFI_REAL)

/*
 * Enlace com a ponte pelo Wi-Fi, no lugar do Bluetooth (só para testes): o robô entra na rede Wi-Fi da casa
 * e abre um servidor TCP na porta WIFI_PORTA. A ponte do notebook conecta em socket://ratatouille.local:3333
 * (ou no IP do robô) e fala o mesmo contrato v1, linha a linha, como pela serial. A HAL e a telemetria não
 * mudam.
 *
 * O nome e a senha da rede ficam em include/config/wifi_local.h, que não vai para o Git (modelo em
 * wifi_local.exemplo.h). Sem o arquivo o firmware compila, mas não entra em nenhuma rede.
 *
 * Quem chama é a tarefa de telemetria (núcleo 0). hal_bt_ler_byte nunca bloqueia, e hal_bt_conectado diz
 * se há uma ponte conectada. Uma conexão nova substitui a anterior (a ponte reabre sozinha quando cai).
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "hal_bt.h"

#if __has_include("config/wifi_local.h")
#include "config/wifi_local.h"
#else
#define WIFI_SSID  ""
#define WIFI_SENHA ""
#define WIFI_SEM_CONFIGURACAO
#endif

#define WIFI_PORTA        3333
#define WIFI_NOME_REDE    "ratatouille" /* ratatouille.local no mDNS */
#define WIFI_AVISO_MS     5000          /* intervalo dos avisos pelo USB enquanto não conecta */

static WiFiServer servidor(WIFI_PORTA);
static WiFiClient cliente;
static bool iniciado;
static bool rede_anunciada;
static uint32_t ultimo_aviso_ms;

bool hal_bt_iniciar(const char *nome_dispositivo)
{
    (void)nome_dispositivo;
#ifdef WIFI_SEM_CONFIGURACAO
    Serial.println("# wifi: falta include/config/wifi_local.h (copie wifi_local.exemplo.h e preencha)");
    return false;
#else
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_NOME_REDE);
    WiFi.setSleep(false); /* sem economia de energia: a telemetria chega sem atraso */
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
    servidor.begin();
    servidor.setNoDelay(true);
    iniciado = true;
    Serial.printf("# wifi: entrando na rede \"%s\"\n", WIFI_SSID);
    return true;
#endif
}

/* Avisa pelo USB quando entra na rede (com o IP) e anuncia ratatouille.local uma vez. */
static void acompanhar_rede(void)
{
    if (WiFi.status() == WL_CONNECTED) {
        if (!rede_anunciada) {
            rede_anunciada = true;
            MDNS.begin(WIFI_NOME_REDE);
            MDNS.addService("ratatouille", "tcp", WIFI_PORTA);
            Serial.printf("# wifi: conectado, IP %s, porta %d (socket://%s.local:%d)\n",
                          WiFi.localIP().toString().c_str(), WIFI_PORTA, WIFI_NOME_REDE, WIFI_PORTA);
        }
    } else {
        rede_anunciada = false;
        if (millis() - ultimo_aviso_ms >= WIFI_AVISO_MS) {
            ultimo_aviso_ms = millis();
            Serial.printf("# wifi: sem rede ainda (status %d)\n", (int)WiFi.status());
        }
    }
}

bool hal_bt_conectado(void)
{
    if (!iniciado) {
        return false;
    }
    acompanhar_rede();
    if (servidor.hasClient()) {
        /* A ponte reconectou: a conexão nova vale, a antiga é fechada. */
        if (cliente) {
            cliente.stop();
        }
        cliente = servidor.available();
        cliente.setNoDelay(true);
        Serial.println("# wifi: ponte conectada");
    }
    return cliente && cliente.connected();
}

size_t hal_bt_escrever(const char *dados, size_t tamanho)
{
    if (!hal_bt_conectado()) {
        return 0;
    }
    return cliente.write(reinterpret_cast<const uint8_t *>(dados), tamanho);
}

int hal_bt_ler_byte(void)
{
    if (!iniciado || !cliente || !cliente.available()) {
        return -1;
    }
    return cliente.read();
}

#endif
