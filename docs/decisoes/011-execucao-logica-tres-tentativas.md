# ADR-011 — Execução lógica, três tentativas e dez minutos totais

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

O TAP fixa **10 minutos por desafio** e menciona **tentativas** de forma genérica. O Glossário e o 4.4 operacionalizam: **execução lógica** com até **3 tentativas**, **10 min totais** desde Nova execução (intervalos incluídos), tipo de labirinto só no backend (RF27).

## Decisão

- **Execução lógica** (`em_andamento` | `concluida` | `cancelada`): aberta por **Nova execução** (UC01); nova execução **cancela** a anterior em andamento.
- **Tentativa** (`health-check` → `running` → `success` | `failed`): no máximo **uma aberta** por vez (RF18); até **3** por execução lógica.
- **Sucesso** de tentativa → execução `concluida`.
- **3 tentativas** `failed` sem sucesso → execução `cancelada`.
- **Link 10 s** ou **tempo 600 s** → tentativa aberta `failed` + execução `cancelada` (UC12.2 / UC12.1).
- **Encerrar tentativa** manual → tentativa `failed`, execução permanece `em_andamento` se couber retomada (RF30).

Regras tabuladas em [transicoes-execucao.md](transicoes-execucao.md) (oráculo BACK-03 e teste 4.2).

## Consequências

- `GerenciadorExecucoes`: transações atômicas ao mudar contadores (BACK-03).
- Front: rótulo *n*/3 e botões conforme RF40.
- Interpretação formal do limite TAP para integração (três labirintos × até 3 tentativas × 10 min por execução lógica).
