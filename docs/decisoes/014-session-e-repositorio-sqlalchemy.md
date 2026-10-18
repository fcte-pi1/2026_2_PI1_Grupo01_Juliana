# ADR-014 — `Session` do SQLAlchemy e `Repositorio` no backend

**Status:** Aceito  
**Data:** 2026-10-14

## Contexto

O backend (BACK-01) concentra consultas e persistência de domínio em **`Repositorio(sessao)`** e **`ExecucaoRepository`**, que encapsulam a **`Session`** do SQLAlchemy (`add`, `flush`, `SELECT … FOR UPDATE`, eager load, etc.).

No BACK-04, o recálculo de métricas (`atualizacao_metricas`) passou a receber **`Session`** diretamente: queries ad hoc sobre `LeituraTelemetria`, `PassoTrajeto` e campos da `Tentativa`, acionadas a partir do gerenciador e da ingestão.

Isso deixou métodos como `verificar_monitores` e `aplicar_evento` usando **`sessao`** em `finalizar_tentativa` / `_fechar_tentativa_falha` e **`repo`** em `_registrar_falha`, `_ultima_celula` e buscas — na mesma transação.

## Decisão

- **`Session`** é a unidade de trabalho do SQLAlchemy (transação por request até `commit`/`rollback`). **`Repositorio`** não é outro armazenamento: é **`Repositorio(sessao)`**, a mesma sessão com API de domínio.
- **Manter as duas formas de chamada**, sem unificar todo o código em `Session` apenas por consistência superficial:
  - **`Repositorio` / `ExecucaoRepository`**: fluxos já modelados (gerenciador, ingestão, histórico BACK-06, locks, `salvar` + `flush`).
  - **`Session` nos serviços de métricas**: recálculo parcial/final que hoje vive em `atualizacao_metricas` (e equivalentes futuros).
- Onde os dois aparecem no mesmo método, **`repo = Repositorio(sessao)` no início**; passar `repo` para helpers que já exigem repositório e `sessao` para `finalizar_tentativa` / `_fechar_tentativa_falha` é **correto**, não redundância de dados.
- **Não** remover a camada `app/repositories/` em favor de SQL espalhado nos serviços: o custo de refactor supera o benefício (ver discussão interna, 2026-10).

## Consequências

- Revisores podem ver `sessao` e `repo` no mesmo bloco (ex.: monitores RF32 / `link_lost`): explicar pelo papel (métricas vs persistência de falha/consulta), não exigir troca mecânica.
- Novas queries de **domínio reutilizável** (buscas, locks, listagens de API) devem preferir **`app/repositories/`**; queries **locais** ao cálculo de métricas podem continuar em serviço com `Session`.
- Testes de integração continuam dependendo de `Repositorio` onde já montam execução/tentativa; métricas testam via ingestão ou unitários em `calculo_metricas` / `atualizacao_metricas`.
