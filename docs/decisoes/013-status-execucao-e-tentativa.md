# ADR-013 — Dois status no contrato: execução e tentativa

**Status:** Aceito  
**Data:** 2026-10-07

## Contexto

A US18 e o dicionário de dados do [4.4](../4.4%20-%20Projeto%20conceitual%20de%20software.md) dão o nome `status` a dois domínios:

- **Execução lógica:** `em_andamento`, `concluida` ou `cancelada`.
- **Tentativa:** `health-check`, `running`, `success` ou `failed`.

No banco, a tabela qualifica a coluna (`EXECUCAO_LOGICA.status` e `TENTATIVA.status`). No contrato HTTP da View com o backend (`src/backend/openapi.yaml`, ARQ-02), os dois valores às vezes viajam no mesmo JSON. Um campo `resultado` (`success` | `failed`) chegou a ser cogitado no resumo do histórico para evitar essa colisão. Ele não existe no dicionário.

## Decisão

- Não há campo `resultado` no contrato. O desfecho da tentativa é o `status` dela. O histórico lê o `status` da execução lógica.
- Em `ResumoExecucao`, `ExecucaoDetalhe` e `Tentativa`, a chave JSON continua `status`. O objeto qualifica o domínio, como a tabela qualifica a coluna. O schema é `StatusExecucao` na execução e `StatusTentativa` dentro de `tentativas`.
- No evento SSE `status`, os dois valores são irmãos no mesmo JSON. As chaves são `status_tentativa` e `status_execucao`.
- Nos eventos `falha` e `sucesso` só vai `status_execucao`. O tipo do evento já diz o desfecho da tentativa (`failed` ou `success`). A tela precisa saber se a execução continua `em_andamento`, passa a `concluida` ou a `cancelada`.

As colunas do PostgreSQL permanecem `status`. O formato da linha serial continua o do [Contrato de telemetria](../4.4.1%20-%20Contrato%20de%20telemetria.md). Esta decisão vale para o JSON da API e do SSE.

## Consequências

- O OpenAPI em `src/backend/openapi.yaml` é a fonte dessa nomenclatura para o front gerar tipos.
- O front provisório ainda tem `resultado` em `ResumoExecucao`. O alinhamento à geração de tipos fica para a tarefa de View, sem alterar esta decisão.
- `GerenciadorExecucoes` continua com as duas máquinas de estado de [transicoes-execucao.md](transicoes-execucao.md).
