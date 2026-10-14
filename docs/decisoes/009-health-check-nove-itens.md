# ADR-009 — Health-check com nove componentes fixos

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

RF29 (Should) exige verificação antes do trajeto, mas a lista de componentes estava pendente no 4.4 e no DER (`HEALTH_CHECK_ITEM`).

## Decisão

Lista **fixa** de **9** itens, um `hc_item` por componente na telemetria (ARQ-01):

1. `bateria`
2. `tof_frontal_esq`
3. `tof_frontal_dir`
4. `tof_esquerdo`
5. `tof_direito`
6. `motor_esquerdo`
7. `motor_direito`
8. `encoder_esquerdo`
9. `encoder_direito`

Reprovação no health-check inicial ou na retomada → tentativa `failed`, motivo `health_check_failed` ou `falha_componente` conforme RF15.

## Consequências

- DER CHECK em `componente` no 4.4; FIRM-09; BACK-02 grava `HEALTH_CHECK_ITEM`.
- AP7: Eletrônica valida cobertura física dos quatro ToF e dos dois encoders.
