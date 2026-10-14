# ADR-010 — Reconexão ≤10 s e deduplicação por `seq`

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

RNF08 exige manter dados após queda de comunicação; RF31 encerra após **10 s** sem mensagens. Sem regra explícita, duplicatas ou buracos corrompem trajeto e métricas.

## Decisão

- Queda **≤ 10 s**: tentativa permanece válida; firmware **reenvia** eventos do buffer (últimos 64, mesmo `seq`); backend **descarta** mensagens com `seq` já persistido (RNF08, RNF-B08).
- Queda **> 10 s**: RF31 / UC12.2 — tentativa `failed` (`link_lost`), execução lógica `cancelada`.
- Medição RNF04/RNF-B04: da **ponte** à tela; trecho Bluetooth medido à parte na bancada.

## Consequências

- FIRM-07 buffer circular; BACK-02 validação de `seq`.
- Teste 7.4 / 4.4: reconexão &lt;10 s mantém tentativa.
