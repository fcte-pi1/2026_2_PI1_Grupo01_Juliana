# ADR-012 — Retomada na célula da falha (sem checkpoint anterior)

**Status:** Aceito  
**Data:** 2026-10-03

## Contexto

O protótipo de interface (telas 8–11 do 4.4) descrevia **checkpoint** como a **célula anterior** à falha (ex.: falha em B2, retomada em B3). Isso divergia de **RF38**, **US31**, o **Glossário**, a tabela `FALHA` (`celula_x`, `celula_y` como ponto de partida da retomada), o BPMN (“recolocar na célula da falha”) e a NVS de posição (**RF42**).

## Decisão

- **Retomar tentativa (*n*/3)** (**UC31**): após tentativa `failed`, a nova tentativa continua na **mesma célula registrada na falha** (`FALHA.celula_x`, `celula_y` / telemetria no momento do `failed`), com **mapa preservado** na execução lógica e na flash do robô.
- O operador **recoloca fisicamente** o robô nessa célula (orientação correta), religa e aciona **Retomar tentativa (*n*/3)**; em seguida **UC20** (health-check) e navegação.
- A interface **não** usa o termo operacional **checkpoint** para uma célula distinta da falha. O protótipo Figma pode manter rótulos antigos nas imagens exportadas; a documentação e a implementação seguem a célula da falha.

## Consequências

- Front (telas de falha e confirmação de retomada): texto e exemplos alinhados à célula da falha (ex. 4×4: falha e retomada em **B2**).
- Backend: ponto de partida da retomada = célula da falha da tentativa anterior (já modelado em `FALHA`).
- Firmware: posição na NVS ao falhar coincide com a célula de retomada; QA-05 / oráculo BACK-03 validam retomada na mesma coordenada.
- Protótipo narrativo do 4.4 revisado; versão futura do Figma pode renomear telas “Retomar do checkpoint” para **Retomar tentativa**.
