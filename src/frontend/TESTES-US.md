# Testes de frontend por história de usuário — #236

Fontes: [backlog e casos de uso](../../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md), [contrato](../../docs/4.4.1%20-%20Contrato%20de%20telemetria.md), [OpenAPI](../backend/openapi.yaml), rotas atuais do backend e [revisão do PR #226](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/pull/226#pullrequestreview-5623122044).

O contrato atual prevalece sobre os trechos antigos das US que colocam health-check dentro da tentativa: a aprovação é prévia à criação e à retomada. A velocidade instantânea chega em mm/s, a média em m/s e a tensão em mV. O frontend apresenta os cálculos recebidos; não comprova sua execução no backend.

## Matriz de cobertura

| Entrega | US verificadas na interface | Suítes e critérios |
| --- | --- | --- |
| FRONT-01 — rotas e cliente | US03, US21, US26–US28 | `rotas`, `semConexao`, `aceitacaoUS`: navegação, 404, indisponibilidade, histórico vazio e cancelamento de consulta do filtro anterior; `e2e/aceitacao-us`: início → histórico → detalhe → volta e registros de cada labirinto antes/depois de recarregar. |
| FRONT-02 — início e criação | US12, US18–US21, US32 (pedido HTTP) | `front02`, `healthCheckFluxo`, `useHealthCheck`: três labirintos, DIP, aprovação válida, criação com IDs do 201, bloqueio de duplicação, recuperação do GET e recusas sem alterar formulário ou contador. |
| FRONT-03 — painel ao vivo | US01, US04, US05, US08, US13–US15, US18, US23–US24 | `front03`, `estadoInicio`, `unidadesContrato`, `aceitacaoUS`: SSE nomeado, alerta de 5 s, reconexão, amostras antigas, relógio do servidor, métricas finais; perda de link permite retomar após T1/T2 e cancela após T3; tempo esgotado cancela em qualquer tentativa. Resultados cancelados são consultados pelo ID, conforme o backend. |
| FRONT-04 — mapa | US06, US11, US17, US28–US31 | `labirinto`, `front04Integracao`, `aceitacaoUS`: ordem do trajeto, orientação dos três labirintos, paredes, deduplicação, posição e falha sem primeiro passo; nove motivos e oito componentes persistidos. |
| FRONT-05 — tentativas | US07, US17–US22, US30–US32 | `front05`, `healthCheckFluxo`, `rotas`, `e2e/front05`, `e2e/encerrar-execucao`: limite de três tentativas, motivo obrigatório, observação ≤100 caracteres, checklist, health-check anterior, retomada da célula da falha, IDs preservados, recusa e bloqueio durante envio. |
| FRONT-06 — histórico/detalhe | US02, US03, US05, US09, US16, US25–US28 | `front06`, `rotas`, `aceitacaoUS`, `e2e/front06`, `e2e/aceitacao-us`: páginas de 100, fim com página vazia, falha na página seguinte, ordem, filtros e detalhe; T1/T2/T3/Todas preservam métricas, tabela, health-check, falhas e percurso completo ao voltar a Todas. |

Os nomes acima são arquivos em `src/testes/*.test.ts(x)`, salvo os caminhos `e2e/` indicados. `mocksFluxo` verifica somente a coerência dos exemplos locais.

## Executar

Na pasta `src/frontend`, usando as dependências já existentes:

```powershell
npm.cmd test
npm.cmd run lint
npm.cmd run build
npm.cmd run test:e2e
```

Somente os novos testes:

```powershell
npm.cmd test -- src/testes/aceitacaoUS.test.tsx
npm.cmd run test:e2e -- e2e/aceitacao-us.spec.ts
```

O Playwright local habilita MSW e salva a captura `us28-detalhe.png` nos resultados do teste de navegação. Os IDs e registros de paginação são exclusivos do teste; reutilizam os exemplos existentes e não representam dados reais.

## Integração real e limites

`npm.cmd run test:e2e:conectado` executa a suíte existente sem MSW, com backend em `127.0.0.1:8000`, banco configurado, `make`, compilador do simulador de navegação e ambiente Python do simulador de telemetria. Essa suíte cria e encerra execuções no backend; usar banco de testes. Não executada nesta entrega: `/back-health` não estava acessível.

US10, US19, US25 e US29 envolvem ingestão, unicidade e persistência reais; os testes isolados verificam somente o consumo das respostas. US13–US15 e US16 dependem de cálculos do backend. Os limites automáticos de US23/US24 e o envio Bluetooth de US32 não são comprovados por respostas controladas. O teste SSE local não comprova latência desde o COMMIT nem operação do robô físico.

Validação local em 24/10/2026, após a segunda revisão: 249 testes Vitest (219 existentes + 30 novos), 13 E2E com MSW (9 existentes + 4 novos), lint e build. O MSW emite avisos ao escrever em streams já fechados durante navegação. Após os cenários passarem, foi necessário encerrar o processo Vite iniciado pela suíte para concluir o teardown no Windows; o Playwright terminou com código 0 e `13 passed`. Não há validação do transporte real nesta execução.

A revisão corrigiu o mock de execução cancelada no endpoint de execução ativa, a verificação do health-check por uma referência DOM antiga, a validação dos filtros apenas por quantidade e a ausência de componente no cenário normal de `falha_componente`. Acrescentou respostas atrasadas de filtros e os estados automáticos nas três tentativas. Os dados da matriz recortam o trajeto e preservam a coerência dos horários de cada tentativa.

Base: branch criada de `origin/dev` (`1428f2b`), com PR #226 integrado. `main` local atualizada com `origin/main` (`0995388`), ainda anterior às entregas de frontend, backend e contrato da dev. A correção #211 (`93b71bc`) permanece na branch `fix/211-unidades-metricas-frontend`, fora da base desta issue. Não foram alterados arquivos de produção, backend ou dependências. O PDF de onboarding não foi localizado no workspace.
