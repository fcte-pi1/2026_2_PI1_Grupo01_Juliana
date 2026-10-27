# Guia dos testes E2E

Os testes E2E rodam no Chromium com o [Playwright](https://playwright.dev) e exercitam a interface como o operador a usa. Há duas suítes. O roteiro funcional completo, com as histórias de usuário e as telas do protótipo de cada caso, está na seção "Testes E2E (sistema)" do [7.4 - Testes de software](../../../docs/7.4%20-%20Testes%20de%20software.md).

| | Suíte com mock | Suíte conectada |
|---|---|---|
| **Comando** | `npm run test:e2e` | `npm run test:e2e:conectado` |
| **Configuração** | `playwright.config.ts` | `playwright.conectado.config.ts` |
| **Dados** | API simulada pelo MSW (`VITE_API_MOCK=true`), front em `:4174` | Back-end real em `http://127.0.0.1:8000`, front sem mock em `:4176` |
| **Arquivos** | `front05`, `front06`, `encerrar-execucao`, `rnf-b03` | `us-conectado.spec.ts` |
| **Testes** | 10 | 11 |
| **Duração** | ~15 s | ~2 min |
| **Roda no CI** | Sim (`frontend.yml`) | Não |
| **Resultado em 26/10/2026** | 10 de 10 aprovados | 10 de 11 aprovados (o E2E-04 reprova, ver abaixo) |

A suíte com mock confere a interface isolada: ela mostra o que a API *deveria* devolver. Só a suíte conectada passa pela cadeia real (front, API e banco) e serve como E2E de sistema.

## Pré-requisitos

**Suíte com mock:** `npm install` e, na primeira vez, `npm run test:e2e:install` (baixa o Chromium).

**Suíte conectada**, além do anterior:

1. **API e Postgres no ar.** Em `src/backend`, `docker compose up -d`. O teste confere `http://127.0.0.1:8000/back-health` antes de começar.
2. **`make` e um compilador C**, que compilam o simulador de navegação do firmware (`src/firmware/simulador-navegacao`).
3. **`uv`**, que roda o simulador de telemetria (`src/simulador-telemetria`).
4. **Banco descartável.** Os testes gravam no banco que a API usa, e as execuções de teste ficam no histórico. Não rode contra um banco que tenha dados que importem.

## Como a suíte conectada funciona

1. Antes dos testes, o `beforeAll` compila o simulador do firmware e grava uma corrida completa do labirinto `4x4-01-dir`, que termina em sucesso. A gravação é fatiada em `hc` (health-check), `trecho` (A1 a A2), `continuacao` (até A3) e `resto` (até o objetivo).
2. Cada teste usa o simulador de telemetria para reproduzir as fatias pela API (`reproduzir(arquivo, boot)`) enquanto o navegador opera a interface.
3. O teste confere a tela e, em seguida, o que a API gravou (`execucaoAtual()` e `detalhe(id)`).
4. Os ganchos `beforeEach` e `afterEach` cancelam a execução aberta, confirmam uma interrupção pendente e concluem um pedido de health-check aberto, para um teste não herdar o estado do anterior.

**Funções de apoio** de `us-conectado.spec.ts`: `prepararLargada` (health-check e Nova execução), `reproduzir`, `pulso` (batimento de telemetria), `retomar`, `encerrarComMotivo`, `hcVariado` (health-check alterado), `mascarasEnviadas`, `execucaoAtual`, `detalhe` e `bootNovo`.

**Duas regras do back-end que afetam os testes:**
- A tentativa cai por `link_lost` depois de 10 s sem telemetria. Testes que esperam mais que isso precisam manter o `pulso`.
- Um health-check de `boot` maior que o da tentativa aberta é tratado como reinício do robô e encerra a tentativa. Dentro de um teste, use o mesmo `boot` do health-check.

## Testes da suíte conectada

| ID | O que confere | Resultado |
|---|---|---|
| E2E-01 | Execução completa com sucesso, do health-check à conclusão | Aprovado |
| E2E-02a | Sem health-check válido (DIP divergente e reprovado), a Nova execução não abre | Aprovado |
| E2E-02b | Nova execução com uma execução em andamento cancela a anterior | Aprovado |
| E2E-03 | Encerramento manual e retomada na célula da falha | Aprovado |
| E2E-04 | Falha de componente e retomada | **Reprovado** |
| E2E-05 | Três falhas manuais escondem a retomada | Aprovado |
| E2E-05b | Execução cancelada não oferece retomada | Aprovado |
| E2E-06 | 10 s sem telemetria encerram a tentativa por `link_lost` | Aprovado |
| E2E-08 | Histórico, filtro e as três tentativas do 4x4 | Aprovado |
| E2E-09 | Paredes enviadas pelo robô nos lados certos de cada célula | Aprovado |
| E2E-10 | Health-check pela web: 30 s sem resposta libera nova solicitação | Aprovado |

O E2E-07 (limite de 10 minutos) não foi realizado; ele é coberto no nível de integração (`test_monitores_relogio`).

**Por que o E2E-04 reprova:** ele reproduz o roteiro `falha-componente` com um `boot` novo, e o simulador de telemetria começa a correr logo depois do health-check, sem esperar o `largar`. A API entende o health-check do `boot` maior como reinício do robô e fecha a tentativa como `link_lost`. É o problema da issue [#249](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/249), no simulador, e não no front.

## Comandos úteis

```bash
# um teste só (por trecho do título)
npx playwright test -c playwright.conectado.config.ts -g "E2E-06"

# resultado sem os blocos de erro, útil para tirar print
npm run test:e2e:conectado 2>&1 | awk '/^  [0-9]+\) /{skip=1} /^  [0-9]+ (failed|passed|flaky|skipped)/{skip=0} !skip'

# ver o trace de um teste que falhou
npx playwright show-trace test-results/<pasta-do-teste>/trace.zip

# modo interativo (suíte com mock)
npm run test:e2e:ui
```

Quando um teste falha, o Playwright guarda o screenshot, o trace e um resumo do erro em `test-results/`. A pasta não é versionada e é recriada a cada rodada.

## Problemas comuns

| Sintoma | Causa e solução |
|---|---|
| `O backend em http://127.0.0.1:8000 não respondeu ao health-check` | API fora do ar. Suba com `docker compose up -d` em `src/backend`. |
| Erro ao rodar `make build/simulador` | Falta `make` ou um compilador C. |
| Porta `4176` em uso | A suíte conectada não reaproveita servidor: pare o outro servidor do front que estiver nessa porta. A suíte com mock reaproveita um servidor já aberto na `4174`. |
| A suíte conectada demora ou expira | Os testes de tempo esperam de verdade (10 s no E2E-06 e 30 s no E2E-10). Rode com a máquina livre. |
| Erros `Cannot enqueue a chunk into a closed readable stream` no log da suíte com mock | Ruído do MSW ao fechar um stream; não afeta o resultado dos testes. |

## Como acrescentar um teste

1. Comece o título com o ID do caso do roteiro (`E2E-11: ...`), para manter a ligação entre o roteiro e a automação.
2. Reaproveite as fatias de `gravacao` e as funções de apoio. Dentro de um teste, use um único `boot`.
3. Não deixe tentativa aberta no fim; os ganchos limpam o estado, mas um teste que falha no meio depende deles.
4. Antes de abrir o PR, rode `npx tsc --noEmit -p tsconfig.e2e.json` e `npx eslint e2e --max-warnings 0`.
5. Atualize o roteiro no 7.4.
