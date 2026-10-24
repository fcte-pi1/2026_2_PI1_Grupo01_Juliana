# ADR-017 — Downlink: três comandos do contrato

**Status:** Aceito  
**Data:** 2026-10-21

## Contexto

A [ADR-006](006-downlink-apenas-interrupcao.md) previa a partida só pelo firmware, logo após o health-check aprovado. A interrupção era o único downlink.

No PR #209 (BACK-04), a discussão sobre contar o tempo da tentativa a partir de `running` levou a desacoplar o health-check da tentativa. Ele virou pré-requisito, com validade de 7 minutos. A largada passou a ser dada pelo operador. Com isso, a interrupção deixou de ser o único comando que desce ao robô.

## Decisão

- O downlink se limita aos três comandos do contrato v1:
  - `interromper`: encerramento manual da tentativa (RF30).
  - `largar`: o operador clica em **Nova execução** ou **Retomar**. O robô só parte com o último health-check aprovado.
  - `health_check`: o operador pede um novo health-check com o robô parado, fora de tentativa.
- Todos vão na resposta ao `POST /telemetria` até o robô mostrar que atendeu ([Contrato de telemetria](../4.4.1%20-%20Contrato%20de%20telemetria.md), seção Comando da web para o robô).
- Continua sem teleoperação, sem comandos de navegação e sem par iniciar/parar genérico (RNF06).
- Encerramentos automáticos (**UC12.1**, **UC12.2**) não enviam comando ao robô.

## Consequências

- RF39, RNF06, RNF-B06, UC30, 4.4, 4.4.1, Glossário e `src/README.md` revisados na issue #230.
- A ponte serial (BACK-08) escreve na serial só os comandos do contrato.
- O firmware espera o `largar` depois do health-check aprovado.
