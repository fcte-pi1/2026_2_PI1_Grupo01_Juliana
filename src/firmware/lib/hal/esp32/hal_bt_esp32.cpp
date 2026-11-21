#if !defined(HAL_SIMULADA) && defined(ARDUINO) && defined(BT_REAL)

/*
 * Bluetooth clássico (SPP) da ESP32 pelo BluetoothSerial: o notebook pareia com o robô e abre
 * /dev/rfcomm0 (ou COMx), e a ponte fala o mesmo contrato v1 que fala pela serial USB.
 *
 * Quem chama é a tarefa de telemetria (núcleo 0). hal_bt_ler_byte nunca bloqueia, e
 * hal_bt_conectado diz se há um notebook na outra ponta.
 */

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <esp_bt.h>

#include "hal_bt.h"

static BluetoothSerial bt;
static bool iniciado;

/*
 * Avisos pelo USB (linhas com #, que a ponte ignora). O callback roda na tarefa do Bluetooth, com pilha
 * pequena: ele só guarda o evento, e quem imprime é hal_bt_conectado, na tarefa de telemetria.
 */
static volatile int evento_pendente; /* 0 nada, 1 conectou, 2 caiu, 3 pareou, 4 pareamento falhou */
static volatile int status_pendente;
/* Congestionamento do canal e escritas que falharam: guardados no callback e impressos na tarefa de telemetria. */
static volatile bool cong_inicio_novo, cong_fim_novo, escrita_falhou_nova;
static volatile uint32_t cong_inicio_ms, cong_fim_ms;
static volatile int escrita_status;
/* Bytes entregues à fila da biblioteca contra bytes que a pilha Bluetooth confirmou ter enviado. */
static volatile uint32_t bytes_enfileirados, bytes_confirmados;
static uint32_t ultimo_resumo_ms;

static void evento_spp(esp_spp_cb_event_t evento, esp_spp_cb_param_t *param)
{
    if (evento == ESP_SPP_SRV_OPEN_EVT) {
        evento_pendente = 1;
    } else if (evento == ESP_SPP_CLOSE_EVT) {
        status_pendente = (int)param->close.status;
        evento_pendente = 2;
    } else if (evento == ESP_SPP_CONG_EVT) {
        if (param->cong.cong) {
            cong_inicio_ms = millis();
            cong_inicio_novo = true;
        } else {
            cong_fim_ms = millis();
            cong_fim_novo = true;
        }
    } else if (evento == ESP_SPP_WRITE_EVT) {
        if (param->write.status == ESP_SPP_SUCCESS) {
            bytes_confirmados += (uint32_t)param->write.len;
        } else {
            escrita_status = (int)param->write.status;
            escrita_falhou_nova = true;
        }
    }
}

static void avisar_evento(void)
{
    if (cong_inicio_novo) {
        cong_inicio_novo = false;
        Serial.printf("# bt: canal congestionado em t=%lu ms\n", (unsigned long)cong_inicio_ms);
    }
    if (cong_fim_novo) {
        cong_fim_novo = false;
        Serial.printf("# bt: canal livre em t=%lu ms (congestionado por %lu ms)\n", (unsigned long)cong_fim_ms,
                      (unsigned long)(cong_fim_ms - cong_inicio_ms));
    }
    if (escrita_falhou_nova) {
        escrita_falhou_nova = false;
        Serial.printf("# bt: escrita falhou, status %d, t=%lu ms\n", escrita_status, (unsigned long)millis());
    }
    if (iniciado && bt.hasClient() && millis() - ultimo_resumo_ms >= 2000) {
        ultimo_resumo_ms = millis();
        Serial.printf("# bt: t=%lu ms enfileirado=%lu B confirmado=%lu B\n", (unsigned long)millis(),
                      (unsigned long)bytes_enfileirados, (unsigned long)bytes_confirmados);
    }
    int e = evento_pendente;
    if (e == 0) {
        return;
    }
    evento_pendente = 0;
    switch (e) {
    case 1: Serial.printf("# bt: cliente conectado em t=%lu ms\n", (unsigned long)millis()); break;
    case 2: Serial.printf("# bt: desconectado (status %d)\n", status_pendente); break;
    case 3: Serial.println("# bt: pareamento concluido"); break;
    case 4: Serial.println("# bt: pareamento FALHOU"); break;
    default: break;
    }
}

bool hal_bt_iniciar(const char *nome_dispositivo)
{
    bt.register_callback(evento_spp);
    bt.onAuthComplete([](boolean ok) { evento_pendente = ok ? 3 : 4; });
    iniciado = bt.begin(nome_dispositivo);
    if (iniciado) {
        /* Potência máxima do rádio clássico: o padrão é baixo e o enlace caía a pouco mais de 3 m. */
        esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);
    }
    Serial.printf("# bt: %s como \"%s\"\n", iniciado ? "aguardando conexao" : "NAO iniciou", nome_dispositivo);
    return iniciado;
}

bool hal_bt_conectado(void)
{
    avisar_evento();
    return iniciado && bt.hasClient();
}

size_t hal_bt_escrever(const char *dados, size_t tamanho)
{
    if (!hal_bt_conectado()) {
        return 0;
    }
    size_t escrito = bt.write(reinterpret_cast<const uint8_t *>(dados), tamanho);
    bytes_enfileirados += (uint32_t)escrito;
    return escrito;
}

int hal_bt_ler_byte(void)
{
    if (!iniciado || !bt.available()) {
        return -1;
    }
    return bt.read();
}

#endif
