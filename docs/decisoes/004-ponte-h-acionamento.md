# ADR-004 — Ponte H dupla para acionamento

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

A V1 usava drivers **A4988** e sinais STEP/DIR para passos. Os motores N20 exigem **PWM**, direção e linha de **standby** (`MOT_STBY`).

## Decisão

**Ponte H dupla** (referência TB6612FNG): um canal por motor N20; controle por PWM + direção; habilitação via `MOT_STBY` para motores desligados na inicialização.

## Consequências

- EAP 3.3.1 descreve ponte H, não A4988.
- Folhas de esquemático do 4.3 devem substituir STEP/DIR/MOT_EN pela interface V2.
- Energia dimensiona pico de corrente dos N20 na ponte H.
