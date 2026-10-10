# Ponte serial Bluetooth (BACK-08)

Processo separado da API: lê a porta serial pareada com a ESP32 (SPP), envia cada linha crua para `POST /telemetria` (`EntradaPonte`) e escreve na serial os JSON de `comandos` da resposta (`RespostaPonte`), sem interpretar o conteúdo.

Implementação prevista na tarefa BACK-08.
