# ADR-015 — Persistir `rumo` da `tel` em `leitura_telemetria`

**Status:** Aceito  
**Data:** 2026-10-15

## Contexto

O contrato 4.4.1 exige `rumo` (`N` | `S` | `L` | `O`) em cada mensagem **`tel`**, para a web orientar o desenho do robô. O schema REST/SSE (`LeituraTelemetria`) já expõe `rumo`, mas o backend gravava só posição, bateria e velocidade — a consulta devolvia sempre `null`.

Isso foi identificado ao validar histórico com o simulador da ponte; **não faz parte do núcleo do BACK-04** (métricas mV / %).

## Decisão

- Adicionar coluna **`rumo`** (nullable, CHECK `N|S|L|O`) em **`leitura_telemetria`**, preenchida na ingestão de **`tel`**.
- **`consulta_execucoes`** passa a mapear o valor persistido para o detalhe da execução.
- **`eixo_longo` por leitura** permanece fora deste passo (campo continua nullable no contrato; pode vir em ADR futuro se o front exigir replay completo).

## Consequências

- Migration `b7c8d9e0f1a2_rumo_leitura_telemetria` após `consumo_bateria_pct`.
- Leituras antigas permanecem com `rumo` null.
- **Organização Git:** este pacote (migration + model + ingestão + consulta + teste) deve ir em **commit separado** do BACK-04 e do fix RF32 (`verificar_monitores` antes de `registrar_mensagem`), para review e histórico claros — por exemplo: `feat(telemetria): persistir rumo nas leituras`.
