# ADR-009 — Health-check com oito componentes fixos

**Status:** Aceito  
**Data:** 2026-09-30  
**Revisão:** os dois ToF frontais viraram um só (migração `3f1c9a2b7d40`), e a lista passou de 9 para 8 itens.

## Contexto

RF29 (Should) exige verificação antes do trajeto, mas a lista de componentes estava pendente no 4.4 e no DER (`HEALTH_CHECK_ITEM`).

## Decisão

Lista **fixa** de **8** itens, um `hc_item` por componente na telemetria (ARQ-01):

1. `bateria`
2. `tof_frontal`
3. `tof_esquerdo`
4. `tof_direito`
5. `motor_esquerdo`
6. `motor_direito`
7. `encoder_esquerdo`
8. `encoder_direito`

Reprovação no health-check (item reprovado ou resultado `aprovado` = `false`) → o backend recusa abrir a tentativa em **Nova execução** e em **Retomar** (`health_check_reprovado`). Nenhuma tentativa `failed` é criada, porque o health-check é anterior à tentativa ([ADR-017](017-downlink-tres-comandos.md)).

## Consequências

- DER CHECK em `componente` no 4.4; FIRM-09; BACK-02 grava `HEALTH_CHECK_ITEM`.
- AP7: Eletrônica valida cobertura física dos três ToF e dos dois encoders.
