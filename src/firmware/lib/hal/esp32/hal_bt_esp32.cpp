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

#include "hal_bt.h"

static BluetoothSerial bt;
static bool iniciado;

/*
 * Avisos pelo USB (linhas com #, que a ponte ignora). O callback roda na tarefa do Bluetooth, com pilha
 * pequena: ele só guarda o evento, e quem imprime é hal_bt_conectado, na tarefa de telemetria.
 */
static volatile int evento_pendente; /* 0 nada, 1 conectou, 2 caiu, 3 pareou, 4 pareamento falhou */
static volatile int status_pendente;

static void evento_spp(esp_spp_cb_event_t evento, esp_spp_cb_param_t *param)
{
    if (evento == ESP_SPP_SRV_OPEN_EVT) {
        evento_pendente = 1;
    } else if (evento == ESP_SPP_CLOSE_EVT) {
        status_pendente = (int)param->close.status;
        evento_pendente = 2;
    }
}

static void avisar_evento(void)
{
    int e = evento_pendente;
    if (e == 0) {
        return;
    }
    evento_pendente = 0;
    switch (e) {
    case 1: Serial.println("# bt: cliente conectado"); break;
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
    return bt.write(reinterpret_cast<const uint8_t *>(dados), tamanho);
}

int hal_bt_ler_byte(void)
{
    if (!iniciado || !bt.available()) {
        return -1;
    }
    return bt.read();
}

#endif
