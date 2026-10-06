# Ponte serial Bluetooth (BACK-08)

Processo separado da API: lê a porta serial pareada com a ESP32 (SPP), envia telemetria para `POST /telemetria` e repassa a interrupção downlink quando a API sinalizar `interrupcao_pendente`.

Implementação prevista na tarefa BACK-08.
