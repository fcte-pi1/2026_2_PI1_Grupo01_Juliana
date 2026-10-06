# ADR-002 — Quatro sensores ToF (I²C)

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

O TAP lista sensores **infravermelhos binários** (HW-201 / LM393). A V2 adota **Time-of-Flight** para distância em milímetros, centralização no corredor e detecção de paredes (RF20).

## Decisão

**Quatro** sensores ToF no barramento I²C (dois frontais, esquerdo e direito), endereços atribuídos via **XSHUT** na inicialização, conforme [4.3](../4.3%20-%20Projeto%20conceitual%20de%20hardware.md).

## Consequências

- EAP 3.2.1 e RF29 alinhados a **4 ToF** (health-check).
- Firmware: driver ToF (FIRM-03) e calibração de limiar de parede.
- TAP não é reescrito; compra real segue orçamento Eletrônica V2.
