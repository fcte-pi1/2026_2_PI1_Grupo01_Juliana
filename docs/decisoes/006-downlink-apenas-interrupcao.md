# ADR-006 — Downlink: apenas interrupção manual

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

Versões antigas do 4.4 ligavam **Nova execução** e encerramentos automáticos a comandos **iniciar/parar** na serial (UC30 incluído por UC01/UC12.1). O TAP proíbe teleoperação (fora de escopo) e fixa autonomia do robô (RNF06, RNF07).

## Decisão

- **Sem** downlink de iniciar ou parar genérico.
- **Único** downlink: JSON `{"cmd":"interromper"}` (contrato ARQ-01) quando o operador confirma **Encerrar tentativa** (RF30 / RF39 / UC04 → UC30).
- Partida após **health-check** aprovado (**UC20**), só pelo firmware; backend abre tentativa e associa telemetria (UC01).

Encerramentos automáticos (**UC12.1**, **UC12.2**) **não** enviam comando ao robô.

## Consequências

- Diagrama de casos de uso e BPMN do 4.4; BACK-03 e FIRM-07.
- Front: protótipo descreve interrupção no encerramento manual, não “zero comandos”.
- Teste 7.4 / 4.3: interrupção na serial em ≤1 s após encerrar.
