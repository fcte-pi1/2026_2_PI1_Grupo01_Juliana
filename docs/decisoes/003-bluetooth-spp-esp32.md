# ADR-003 — Bluetooth clássico SPP na ESP32

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

O TAP prevê módulo **HM-10 BLE 4.0** externo. A placa **ESP32 DevKit** já integra rádio Bluetooth; um módulo extra ocupa espaço e pinos.

## Decisão

Telemetria uplink por **Bluetooth clássico**, perfil **SPP** (`BluetoothSerial`), recebida como **porta serial** no notebook (ponte → backend). Substitui o HM-10 do TAP (EAP 3.4.1, 4.3, RF08).

## Consequências

- Orçamento: sem linha HM-10 adicional ([6 - Orçamento](../6%20-%20Or%C3%A7amento.md)).
- Contrato serial (ARQ-01) sobre SPP; FIRM-07/FIRM-11 e BACK-08.
- AP7: Eletrônica + Software demonstram enlace ponta a ponta.
