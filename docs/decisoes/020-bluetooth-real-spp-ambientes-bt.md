# ADR-020 — Bluetooth real (SPP) com o mesmo contrato, em ambientes à parte do USB

**Status:** Aceito  
**Data:** 2026-11-21

## Contexto

Na pista o robô anda sem o cabo USB, e a calibração da FIRM-10 e a FIRM-11 ([#139](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/139)) dependem de um enlace sem fio. Até aqui a telemetria saía pela serial USB (`hal_bt_serial_esp32.cpp`), com a HAL de Bluetooth já isolada atrás de `hal_bt.h`. A ponte do backend (BACK-08) já lê o SPP pelo `/dev/rfcomm0`.

## Decisão

- **Driver `hal_bt_esp32.cpp`** com o `BluetoothSerial` (SPP, nome **Ratatouille**), ativado por `-D BT_REAL`. A HAL e a `lib/telemetria` não mudam: o contrato v1 sai igual pelo USB e pelo Bluetooth.
- **Ambientes à parte:** `esp32_real_bt` e `esp32_real_bt_tof_mock` (firmware completo) e `esp32_calibracao_bt` (bancada). Os ambientes sem `_bt` continuam pelo USB, para a bancada e o CI. Os `_bt` usam a partição `huge_app`, porque o app do Bluetooth clássico passa de 1 MB.
- **Bancada de calibração com console duplo:** tudo que a bancada imprime vai ao USB e ao Bluetooth, e as teclas podem chegar por qualquer um, então a calibração na pista não precisa de cabo.
- **Comandos em linhas de uma tecla só na bancada.** O `/dev/rfcomm0`, enquanto está em modo de terminal comum, devolve ao robô tudo o que recebe; o robô lia o eco do próprio banner (`a A g G ...`) como teclas e executava movimentos sozinho (observado em 21/11/2026). A bancada passou a aceitar só linhas com uma letra, e o firmware do robô já só aceita os comandos JSON do contrato.
- **Pareamento no sistema operacional** (`bluetoothctl` e `rfcomm`, no Linux); o robô só expõe o SPP. O roteiro está no README do firmware.

## Alternativas descartadas

- **Gravar o resultado da calibração na flash e ler depois pelo USB.** Funciona sem Bluetooth, mas não deixa ajustar na hora e gasta a flash; o Bluetooth já é necessário para a FIRM-11.
- **Um único firmware com Bluetooth e USB ao mesmo tempo.** O app do Bluetooth não cabe na partição padrão, e o CI e as bancadas ficariam presos à partição grande.

## Consequências

- O enlace Bluetooth só pode ser validado no robô (latência até 3 s, estabilidade a 3 m); o CI compila os três ambientes `_bt`, mas não os executa.
- Na pista, a mais de cerca de 3 m do notebook o enlace desconectou com facilidade (teste 5.27 do 7.5, sem medição): a estabilidade a 3 m ainda não está atendida e é a pendência principal da FIRM-11.
- O endereço do Bluetooth é o da base mais 2 no último byte, e o pareamento é por notebook. A ponte reabre a porta sozinha se a conexão cair.
- O `TOF_MOCK` continua proibido no firmware final.
