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
├── api/               ClienteAPI, endpoints e tipos (provisórios até o contrato OpenAPI do ARQ-02)
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
