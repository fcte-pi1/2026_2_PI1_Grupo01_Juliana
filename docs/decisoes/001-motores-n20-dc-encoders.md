# ADR-001 — Motores N20 DC com encoders

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

O [TAP](../1%20-%20TAP.md) prevê motores de passo Nema 11. Estrutura, Energia e Eletrônica fecharam **arquitetura V2** (menor massa, malha fechada), documentada no [4.3](../4.3%20-%20Projeto%20conceitual%20de%20hardware.md) e na [EAP](../3%20-%20EAP.md) (itens 3.3 e 4.2.3.1.1).

## Decisão

Adotar **dois motores DC N20 com encoder**, controle de velocidade por **PID** e acionamento via **ponte H dupla**. Abandonar motores de passo no produto final; o TAP permanece registro histórico de orçamento V1.

## Consequências

- EAP 3.3, 4.3, RF21 e RF22 descrevem N20 + encoders.
- Esquemáticos ainda em transição V1→V2 devem convergir para PWM/direção e encoders (nota técnica do 4.3).
- AP7: apresentar desvio formal ao TAP com este ADR.
