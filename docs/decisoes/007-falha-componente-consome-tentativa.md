# ADR-007 — Falha de componente consome tentativa

**Status:** Aceito  
**Data:** 2026-09-30

## Contexto

Diagramas antigos do firmware tinham estado **Falha de componente** com nova verificação na **mesma** tentativa (health-check após 5 s, telemetria `retomada` sem incrementar `attempt_index`). Isso conflita com o TAP: até **3 tentativas** por execução lógica e retomada explícita (**UC31** / RF38).

## Decisão

Qualquer **falha de componente** durante a corrida:

1. Firmware para e envia telemetria `failed`, motivo `falha_componente` + componente (RF15).
2. Tentativa encerra e **consome** uma das 3 (RF16).
3. Operador repara e aciona **Retomar tentativa (*n*/3)** → nova `tentativa_id`, `attempt_index + 1`, **UC20**, mapa/NVS preservados (RF38, RF42).

Não existe retomada “silenciosa” na mesma tentativa.

## Consequências

- Removido estado intermediário do firmware (Tabela A/B do 4.4); atualizar figura PNG quando possível.
- BACK-03 e testes 4.2 seguem [transicoes-execucao.md](transicoes-execucao.md).
