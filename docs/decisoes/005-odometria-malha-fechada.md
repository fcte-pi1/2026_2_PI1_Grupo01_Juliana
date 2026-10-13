# ADR-005 — Odometria em malha fechada

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

Motores de passo permitiam estimar deslocamento por pulsos comandados (**malha aberta**). Com N20, derrapagem exige **realimentação** (RF6, RF22).

## Decisão

Medir deslocamento e velocidade pelos **encoders** (PCNT/interrupções na ESP32). Controle de movimento com **PID** de velocidade. No backend, distância para métricas: células × 180 mm (RF12), refinada por telemetria do firmware.

## Consequências

- EAP 3.3.2 e RF21/RF22.
- Detecção de `stuck`: motores acionados e encoders parados além do limiar (US45, Tabela B do 4.4).
- Testes de subsistema validam repetibilidade de curvas 90°/180°.
