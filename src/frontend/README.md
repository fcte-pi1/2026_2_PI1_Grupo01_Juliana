# _Frontend_

Interface de telemetria do Ratatouille em [React](https://react.dev/) + [TypeScript](https://www.typescriptlang.org/), com [Vite](https://vite.dev/), [MSW](https://mswjs.io/) para os mocks e [Vitest](https://vitest.dev/) para os testes, conforme o [4.4 — Projeto conceitual de software](../../docs/4.4%20-%20Projeto%20conceitual%20de%20software.md).

## Como rodar

```bash
cd src/frontend
npm install
npm run dev        # http://localhost:5173
```

| Script              | O que faz                                     |
| ------------------- | --------------------------------------------- |
| `npm run dev`       | servidor de desenvolvimento                   |
| `npm run lint`      | ESLint, sem tolerar avisos                    |
| `npm run typecheck` | checagem de tipos (`tsc -b`)                  |
| `npm test`          | testes com Vitest (`npm run test:watch` para modo contínuo) |
| `npm run test:e2e`  | testes E2E no Chromium com Playwright         |
| `npm run build`     | build de produção em `dist/`                  |

Na primeira execução dos testes E2E, instale o navegador com
`npm run test:e2e:install`.

O CI (`.github/workflows/frontend.yml`) roda lint, tipos, testes unitários, testes E2E e build em todo PR que mexe em `src/frontend`.

## Mocks e backend real

A variável `VITE_API_MOCK` decide de onde vêm os dados:

- `true` (padrão em `.env.development`): o MSW intercepta as chamadas para `/api` e responde com os cenários de `src/mocks`. Nada de mock entra no build de produção.
- `false`: as chamadas vão para o FastAPI em `http://localhost:8000` pelo proxy do Vite, que remove o prefixo `/api`. Crie um `.env.development.local` com `VITE_API_MOCK=false` e reinicie o Vite. Uma variável já definida no terminal tem prioridade sobre esse arquivo.

### Cenários do protótipo

Os 12 estados de tela do protótipo podem ser abertos pelo painel flutuante "Mock" no canto da tela ou pela URL:

```
/?cenario=08-encerrada-como-falha
/?cenario=09-retomar-tentativa&modal=retomar
/?cenario=04-concluida&execucao=e-0042
```

Cenários: `01-inicio`, `02-health-check`, `03-em-execucao`, `04-concluida`, `05-execucao-recusada`, `06-sem-comunicacao`, `07-encerrar-execucao`, `08-encerrada-como-falha`, `09-retomar-tentativa`, `10-retomada-tentativa-2`, `11-concluida-apos-retomada`, `12-falha-sem-retomada`. O painel continua permitindo abrir um estado isolado para inspeção visual. A execução é mantida na sessão da aba: Nova execução → health-check → corrida, com encerramento/retomada pelos botões e resultado acessível no histórico e detalhe. A demonstração 4x4 reproduz o trajeto e as leituras já definidos nos mocks; os outros tamanhos aguardam dados de percurso após o health-check.

### FRONT-02 — início e nova execução

No cenário `05-execucao-recusada`, use Nova execução e confirme para receber o aviso pela resposta 409.

A tela consulta `GET /execucoes/em-andamento` (204 quando não há execução) e `GET /execucoes` para a última execução e as marcas dos três labirintos. Não depende de rotas extras para buscar essas informações.

Nova execução envia `POST /execucoes` com `tipo_labirinto`. Havendo uma em andamento, o modal informa o cancelamento antes do envio. O retorno 201 fornece os IDs e `attempt_index`; em seguida, a tela consulta o detalhe por ID e mostra Tentativa 1/3. Se essa consulta falhar, a ação de carregar tenta somente o GET, sem criar outra execução. Uma recusa 409 exibe o motivo recebido e preserva a tela e a seleção.

`src/api/schema.ts` acompanha o contrato OpenAPI integrado à `dev`, incluindo `EntradaPonte` e `RespostaPonte`. Para atualizar os tipos após mudanças no contrato, execute `npm run gerar-tipos`. Os endpoints normalizam os campos opcionais para a View; dados ausentes de leituras, trajeto ou health-check ficam pendentes. O UUID é usado quando a API não informa um número sequencial; os números do protótipo ficam nos mocks.

O backend integrado da `dev` implementa criação, execução atual, detalhe, histórico, encerramento e retomada. Encerrar recebe o detalhe com HTTP 200; retomar recebe os IDs com HTTP 201 e recarrega o detalhe persistido. A troca de telas acompanha esses dados e os eventos SSE, sem depender do parâmetro `cenario` no modo real.

### FRONT-03 — tela de tempo real

O `ClienteAPI` assina os eventos nomeados `telemetria`, `passo`, `status`, `health_check`, `falha` e `sucesso` do contrato. O mesmo `EventSource` faz a reconexão automática e conserva `Last-Event-ID`. Na conexão e após mudanças de estado, o GET de detalhe reidrata a execução; passos repetidos são deduplicados e uma consulta atrasada não apaga amostras recentes. Se o GET falhar, os dados conhecidos ficam visíveis e a ação **Atualizar dados** permite tentar novamente.

A organização visual acompanha os PNGs do protótipo em `docs/figs/prototipo`: na corrida, cronômetro e três métricas à esquerda e mapa/comunicação à direita; no health-check, verificações e sensores separados; no resultado, mapa completo e métricas laterais. Os dois tempos exigidos pela issue e as três métricas (velocidade, bateria e célula) ficam visíveis durante a corrida sem rolar em 1366×768. Os cronômetros usam as datas do servidor e o relógio corrigido pelo cabeçalho HTTP `Date`; ao término, usam os tempos/datas finais recebidos. O health-check mostra os oito componentes do contrato atual. O alerta começa aos 5 s sem atualização. Conforme a ADR-016 e o OpenAPI, bateria e consumo são exibidos em mV, velocidade instantânea em mm/s e velocidade média em m/s. O health-check mostra bateria em mV e ToF em mm. A formatação preserva a precisão recebida; nenhum valor ausente vira zero.

Ao terminar, a URL conserva `execucao=<id>` e o resultado é restaurado pelo GET de detalhe, mesmo com `/execucoes/em-andamento` retornando 204. O seletor Mock usa essa seleção para abrir os cenários finais. **Voltar ao início** abandona a seleção; **Nova execução** também a remove após carregar o novo registro. A comparação com o melhor tempo aparece no resultado com sucesso; após falha, a tela informa que não houve chegada ao objetivo. As regras atuais das ADRs prevalecem sobre os textos antigos das imagens: retomada na célula da falha e dez minutos por execução lógica, incluindo pausas.

O percentual de carga vem de `bateria_pct` no SSE; o front não calcula a curva da bateria. Consumo percentual (queda em pontos percentuais) e variação contra o melhor tempo vêm do GET e aparecem no encerramento e no detalhe. Campos ausentes permanecem pendentes. O GET não fornece o percentual em cada leitura: após recarregar, ele aguarda o próximo evento de telemetria.

A aprovação do health-check vem do backend; componentes sem resposta permanecem pendentes, e uma reprovação não marca o percurso como concluído. A célula de uma falha persistida prevalece sobre uma leitura anterior ao restaurar o GET. Os tempos arredondados para décimos fazem a virada do minuto corretamente; a tabela conta desde a largada, sem incluir o health-check anterior à tentativa.

Eventos identificados de tentativas anteriores atualizam somente o próprio histórico, sem alterar célula, estado ou horário da tentativa atual. Passos atrasados completam o trajeto sem mover a posição quando antecedem o início da tentativa atual ou chegam após o término. Quando o evento não informa `entrou_em`, a ordenação usa apenas o `passo_id`, conforme os dados disponíveis no contrato.

No modo mock, o estado fica na sessão da aba para o GET restaurar IDs, início, leituras e tentativas após recarregar. As medições usam os exemplos existentes, adequados a mV/mm/s. O stream atende também a execução inicial `e-0042`. A releitura de um `passo_id` atualiza suas paredes sem duplicar o trajeto, preservando falhas e orientação do FRONT-04.

Os testes locais cobrem eventos nomeados, alerta de 5 s, valores/unidades, reconexão, deduplicação, relógio do servidor, valores finais, recarregamento e o fluxo da demonstração. O SSE da BACK-07 já está integrado. O teste com transporte controlado mede apenas do recebimento do evento à atualização da interface; não comprova o limite de 2 s desde o COMMIT no backend. O roteiro para a validação real está em [VALIDACAO-FRONT03.md](VALIDACAO-FRONT03.md).

### FRONT-05 — controle de tentativas (#122)

O `ControleTentativas` oferece **Encerrar tentativa** quando há tentativa aberta e **Retomar tentativa (2/3 ou 3/3)** após a última tentativa falhar, sem outra aberta. Execução cancelada/concluída ou três tentativas usadas ocultam a retomada. O indicador **Tentativa n/3** continua no resumo existente.

O modal Encerrar começa sem motivo selecionado: o operador precisa escolher `collision`, `stuck` ou `out_of_track`. A observação opcional aceita até 100 caracteres, inclusive ao colar. A confirmação de retomada mantém o checklist e a célula da falha do protótipo. Durante o pedido, ficam bloqueados os botões de ação, os campos e o fechamento do modal. O bloqueio de envio existente na página impede POST duplicado; uma recusa preserva o preenchimento e permite tentar novamente.

Os comandos usam os endpoints existentes, sem mudanças no backend: `POST /execucoes/{id}/encerrar-tentativa` e `POST /execucoes/{id}/retomar`. Os testes em `src/testes/front05.test.tsx` cobrem visibilidade, motivo obrigatório, limite da observação, checklist, pedidos pendentes, clique duplo e recusa. Para executar somente essa suíte: `npm run test -- src/testes/front05.test.tsx`.

Encerrar aplica o detalhe persistido retornado pelo HTTP 200, sem depender de outro GET ou de um evento SSE para trocar a tela. Retomar lê os IDs do HTTP 201 e consulta o detalhe dessa execução antes de liberar os controles. Se a tentativa foi aberta e o GET falhar, **Carregar dados da tentativa** repete somente a consulta; não abre outra tentativa. Uma resposta antiga não reabre uma tentativa encerrada nem substitui a tentativa recém-aberta.

Conforme o contrato atual e a revisão da BACK-04 (#209), o health-check é anterior a qualquer tentativa, inclusive à retomada. A preparação consulta `GET /health-check/ultimo` ao abrir e a cada dez segundos em repouso, passando a um segundo enquanto aguarda a verificação, e oferece `POST /health-check/solicitar`. Só libera a confirmação quando a aprovação veio do backend, `valido_ate` está vigente e `tipo_dip` corresponde ao labirinto. A validade acompanha o relógio local corrigido pelo servidor, sem depender da próxima consulta. A seleção na tela não muda o DIP. Uma solicitação HTTP 202 aguarda um resultado com `iniciado_em` a partir de `solicitado_em`; a aprovação antiga não libera a largada. Não se abre nem se consome tentativa durante a verificação. O HTTP 201 já abre a tentativa em `running` e pede a largada ao robô.

Se a verificação não concluir em 30 segundos, a tela informa o erro e libera uma nova solicitação. O prazo também vale para um health-check parcial encontrado ao abrir a página e não reinicia ao trocar o labirinto. Após o limite, as consultas voltam ao intervalo de repouso e a largada continua bloqueada; uma aprovação anterior não é reaproveitada. Um resultado novo concluído pode ser recuperado por consulta posterior. O SSE existente é por execução e não oferece um canal global para essa preparação anterior à largada.

As recusas `health_check_ausente`, `health_check_reprovado`, `health_check_expirado` e `dip_divergente` preservam o formulário e a contagem recebida. O backend continua responsável por validar os comandos. Os tipos de `src/api/schema.ts` foram regenerados do OpenAPI do projeto, sem manter o estado antigo `health-check` dentro da tentativa. Os testes de `healthCheckFluxo.test.tsx` cobrem pré-requisitos ausentes, pendentes, reprovados, expirados, DIP divergente, comunicação indisponível e espera do novo resultado. No modo de demonstração, o cenário 02 representa uma verificação antes da execução; o mock usa os oito exemplos existentes e conserva o resultado na sessão.

Base atualizada por avanço direto com `origin/dev` (`ac59541`), incluindo o FRONT-03 já mesclado. As alterações locais anteriores foram preservadas e reconciliadas com o botão de encerramento completo da execução e os ajustes de mapa que entraram nessa base. As alterações próprias do FRONT-05 estão restritas a `src/frontend`.

Validação local: 209 testes de unidade/componente/integração e 7 E2E, além de lint, tipos e build. Os E2E usam os mocks existentes e conferem health-check anterior à largada, recuperação após 30 segundos sem resultado, motivo obrigatório, limite da observação, um único POST ao encerrar, checklist e preservação da execução, mapa e falha após retomar e recarregar. Para repetir no PowerShell: `$env:VITE_API_MOCK = 'true'; npm.cmd run test:e2e`. A API local está indisponível; os testes com MSW não substituem uma execução com o backend real nem comprovam a latência desde a persistência.

## Rotas

| Rota              | Tela                                                    |
| ----------------- | ------------------------------------------------------- |
| `/`               | Início: nova execução, execução ativa e modais          |
| `/execucoes`      | Histórico, com filtro por labirinto (`?labirinto=4x4`)  |
| `/execucoes/:id`  | Detalhe da execução: trajeto, tentativas e health-check |

## Estrutura

```
src/
├── App.tsx            rotas
├── main.tsx           liga o MSW (se mock) e o roteador
├── api/               ClienteAPI, endpoints, modelos da View e tipos gerados do OpenAPI (schema.ts)
├── mocks/             handlers do MSW, dados e os 12 cenários
├── pages/             telas; inicio/estadoInicio.ts deriva o estado da tela a partir da execução
├── components/
│   ├── layout/        barra lateral, cabeçalho
│   ├── telemetria/    métricas, tempo, leituras, health-check, comunicação
│   ├── tentativas/    modais Encerrar/Retomar, contador, aviso de recusa
│   ├── labirinto/     visualização do trajeto
│   ├── historico/     filtro e tabela de execuções
│   └── execucao/      resumo, etapas, formulário de nova execução, falha
├── hooks/             busca de dados e assinatura do stream SSE
├── utils/             formatação e constantes (limite de 10 min, 3 tentativas)
├── styles/            tokens e estilos globais
└── testes/            Vitest + Testing Library
```

Os componentes só recebem props tipadas e não fazem requisições; a busca de dados fica nos hooks e nas páginas.

> [!WARNING]
> **Não acrescente arquivos referentes ao _backend_ nesta pasta.** Eles deverão ser armazenados na pasta [backend](../backend) deste repositório.
