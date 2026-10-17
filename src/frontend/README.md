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
| `npm run build`     | build de produção em `dist/`                  |

O CI (`.github/workflows/frontend.yml`) roda lint, tipos, testes e build em todo PR que mexe em `src/frontend`.

## Mocks e backend real

A variável `VITE_API_MOCK` decide de onde vêm os dados:

- `true` (padrão em `.env.development`): o MSW intercepta as chamadas para `/api` e responde com os cenários de `src/mocks`. Nada de mock entra no build de produção.
- `false`: as chamadas vão para o FastAPI em `http://localhost:8000` pelo proxy do Vite. Crie um `.env.development.local` com `VITE_API_MOCK=false`.

### Cenários do protótipo

Os 12 estados de tela do protótipo podem ser abertos pelo painel flutuante "Mock" no canto da tela ou pela URL:

```
/?cenario=08-encerrada-como-falha
/?cenario=09-retomar-tentativa&modal=retomar
```

Cenários: `01-inicio`, `02-health-check`, `03-em-execucao`, `04-concluida`, `05-execucao-recusada`, `06-sem-comunicacao`, `07-encerrar-execucao`, `08-encerrada-como-falha`, `09-retomar-tentativa`, `10-retomada-tentativa-2`, `11-concluida-apos-retomada`, `12-falha-sem-retomada`. O cenário escolhido fica salvo na aba (sessionStorage), e os botões da tela avançam o cenário como o backend faria (Nova execução → health-check, Encerrar → falha, Retomar → tentativa 2).

### FRONT-02 — início e nova execução

No cenário `05-execucao-recusada`, use Nova execução e confirme para receber o aviso pela resposta 409.

A tela consulta `GET /execucoes/em-andamento` (204 quando não há execução) e `GET /execucoes` para a última execução e as marcas dos três labirintos. Não depende de rotas extras para buscar essas informações.

Nova execução envia `POST /execucoes` com `tipo_labirinto`. Havendo uma em andamento, o modal informa o cancelamento antes do envio. O retorno 201 fornece os IDs e `attempt_index`; em seguida, a tela consulta o detalhe por ID e mostra Tentativa 1/3. Se essa consulta falhar, a ação de carregar tenta somente o GET, sem criar outra execução. Uma recusa 409 exibe o motivo recebido e preserva a tela e a seleção.

`src/api/schema.ts` acompanha o contrato OpenAPI integrado à `dev`, incluindo `EntradaPonte` e `RespostaPonte`. Para atualizar os tipos após mudanças no contrato, execute `npm run gerar-tipos`. Os endpoints normalizam os campos opcionais para a View; dados ausentes de leituras, trajeto ou health-check ficam pendentes. O UUID é usado quando a API não informa um número sequencial; os números do protótipo ficam nos mocks.

A integração completa depende da implementação de `POST /execucoes` e `GET /execucoes/em-andamento`, ainda pendentes no backend da `dev`. Histórico e detalhe já possuem implementação. Os comandos existentes de encerrar/retomar continuam no escopo das respectivas issues.

### FRONT-03 — tela de tempo real

O `ClienteAPI` assina os eventos nomeados `telemetria`, `passo`, `status`, `health_check`, `falha` e `sucesso` do contrato. O mesmo `EventSource` faz a reconexão automática e conserva `Last-Event-ID`. Na conexão e após mudanças de estado, o GET de detalhe reidrata a execução; passos repetidos são deduplicados e uma consulta atrasada não apaga amostras recentes. Se o GET falhar, os dados conhecidos ficam visíveis e a ação **Atualizar dados** permite tentar novamente.

Os cinco cartões mostram os tempos da tentativa e da execução, célula, bateria e velocidade sem rolar a tela em 1366×768. Os cronômetros usam as datas do servidor e o relógio corrigido pelo cabeçalho HTTP `Date`; ao término, usam os tempos/datas finais recebidos. O health-check fica em bloco separado com os oito componentes do contrato atual. O alerta começa aos 5 s sem atualização. Medições mantêm a precisão recebida: tensão em V, velocidade em m/s e consumo de bateria em V; nenhum valor ausente vira zero.

A aprovação do health-check depende dos oito resultados positivos; componentes sem resposta permanecem pendentes, e uma reprovação não marca o percurso como concluído. A célula de uma falha persistida prevalece sobre uma leitura anterior ao restaurar o GET. Os tempos arredondados para décimos fazem a virada do minuto corretamente; a tabela conta desde o início da tentativa, incluindo o health-check.

Eventos identificados de tentativas anteriores atualizam somente o próprio histórico, sem alterar célula, estado ou horário da tentativa atual. Passos atrasados completam o trajeto sem mover a posição quando antecedem o início da tentativa atual ou chegam após o término. Quando o evento não informa `entrou_em`, a ordenação usa apenas o `passo_id`, conforme os dados disponíveis no contrato.

No modo mock, o estado fica na sessão da aba para o GET restaurar IDs, início e leituras após recarregar. As medições dos cenários de bateria seguem os exemplos `Tela02HealthCheck`, `Tela03EmExecucao`, `Tela04Concluida`, `Tela08EncerradaComoFalha` e `Tela11ConcluidaAposRetomada` do OpenAPI, sem simular novas medições.

Validação local: testes de eventos nomeados, alerta de 5 s, valores/unidades, reconexão, deduplicação, relógio do servidor, valores finais e recarregamento. A verificação de até 2 s nos testes mede do recebimento do evento à atualização da interface, usando transporte controlado. A medição completa com o simulador, desde o COMMIT do backend, depende do BACK-07 (#126): a rota `/stream` ainda retorna 501. Também existe uma divergência no backend atual, que persiste `bat_mv`/`vel_mm_s` e os devolve sem converter para V/m/s; esse ajuste é do backend e deve ser resolvido antes da validação real do RNF-B01.

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
