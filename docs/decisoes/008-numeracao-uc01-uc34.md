# ADR-008 — Numeração única UC01–UC34

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

Requisitos e rascunhos citavam IDs antigos (ex.: UC01 = health-check, UC16 = nova execução, UC21 = timeout/link, UC24 = encerrar) e remetiam a um documento **2.1** externo ao repositório.

## Decisão

Fonte única de IDs: [Diagrama de Casos de Uso](../4.4%20-%20Projeto%20conceitual%20de%20software.md#diagrama-de-casos-de-uso) no **4.4**, com mapeamento estável, por exemplo:

| Papel | ID |
| --- | --- |
| Nova execução | UC01 |
| Encerrar tentativa | UC04 |
| Enviar interrupção | UC30 |
| Retomar tentativa (*n*/3) | UC31 |
| Health-check | UC20 |
| Estouro 10 min | UC12.1 |
| Perda de link 10 s | UC12.2 |

`docs/2 - Requisitos.md` referencia os mesmos IDs.

## Consequências

- Histórias US e RF alinhadas; issues e testes citam UC do 4.4.
- Removida cláusula “prevalece o 2.1” do 4.4.
