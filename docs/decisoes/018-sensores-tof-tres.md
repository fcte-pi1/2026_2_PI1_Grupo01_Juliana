# ADR-018 — Três sensores ToF (I²C)

**Status:** Aceito  
**Data:** 2026-10-07

## Contexto

A [ADR-002](002-sensores-tof-quatro.md) previa quatro sensores ToF: dois frontais, um esquerdo e um direito. Em 07/10/2026, o professor confirmou a montagem com três sensores VL53L0X numa torre, como já mostrava a estrutura do [4.1](../4.1%20-%20Projeto%20conceitual%20de%20estruturas.md).

## Decisão

**Três** sensores ToF no barramento I²C: um frontal centralizado (0°) e dois laterais (esquerdo e direito, a 45°). Os endereços são atribuídos via **XSHUT** (`XSHUT_F`, `XSHUT_E` e `XSHUT_D`) na inicialização, conforme o [4.3](../4.3%20-%20Projeto%20conceitual%20de%20hardware.md).

## Consequências

- Firmware, contrato de telemetria e backend ajustados no PR #183; o health-check passou a ter 8 itens ([ADR-009](009-health-check-nove-itens.md)).
- Energia, Eletrônica, Cronograma e Orçamento revisados na issue #192; folha de Controle do esquemático na issue #202.
- EAP 3.2.1 e 4.3 alinhados a 3 ToF na issue #273.
