# Política de Contribuição — PI1 Grupo 01

Este documento define o fluxo de trabalho de versionamento utilizando GitHub no projeto **2026_2_PI1_Grupo01_Juliana**.

O objetivo é garantir: organização do código, facilidade de colaboração, controle de versões estáveis, segurança no processo de promoção entre ambientes e rastreabilidade entre código, Pull Requests e as áreas do projeto (back, front, firmware, hw, mec).

## 1. Visão Geral

**Ninguém deve trabalhar diretamente nas branches `main` e `dev`.**

Cada integrante cria uma branch para sua tarefa, faz os commits nela e abre um Pull Request para revisão e merge.

Fluxo resumido:

```text
main → criar branch → commits → PR → dev → validação → PR → main
```

## 2. Estratégia de Branches

- **main** — Branch principal e padrão do repositório. Representa o estado mais próximo de uma versão estável/entrega. Atualmente reflete homologação; futuramente deverá refletir a versão final entregue.
- **dev** — Branch de testes e validação. Utilizada para consolidar funcionalidades antes da promoção para a main.
- **gh-pages** — Branch dedicada ao deploy da documentação do repositório via GitHub Pages. Não participa do fluxo de desenvolvimento (main → dev → branch); é atualizada automaticamente por uma GitHub Action, disparada a cada merge na main.

## 3. Padrão de Criação de Branches

Toda nova implementação deve ser criada a partir da **main**.

**Tipos:**

- `feature/*` → novas funcionalidades
- `refactor/*` → melhorias internas sem alteração de regra de negócio
- `fix/*` → correções pontuais
- `bugfix/*` → correções de bugs identificados

**Convenção:** `<tipo>/<tag>-<descrição-curta>`

**Tags de área:** `back` | `front` | `firmware` | `hw` | `mec` | `docs`

**Exemplos:**

- `feature/back-login`
- `fix/front-ajuste-layout-dashboard`
- `refactor/firmware-otimizacao-sensor`
- `bugfix/back-correcao-timeout-api`
- `feature/hw-especificacao-placa`
- `fix/mec-ajuste-tolerancia-encaixe`
- `feature/docs-atualizar-readme`

**Regras:** letras minúsculas, palavras separadas por hífen, incluir a tag da área (back/front/firmware/hw/mec/docs) logo após o tipo, manter nome curto e rastreável.

Evite nomes genéricos como `teste`, `coisa`, `mudancas` ou `branch-luiza`.

### Como criar a branch

Antes de criar uma branch, atualize sua cópia da `main`:

```bash
git switch main
git pull origin main
git switch -c feature/back-minha-tarefa
```

## 4. Fluxo de Desenvolvimento

1. Criar branch a partir da main
2. Desenvolver a funcionalidade ou correção
3. Realizar commits seguindo o padrão definido
4. Abrir PR da branch de trabalho para dev
5. Validar comportamento em dev
6. Após aprovação e testes, abrir PR de dev para main

**Fluxo resumido:** `main → branch → PR → dev → validação → PR → main`

### Enviando seu trabalho

```bash
git add .
git commit -m "feat: minha alteração"
git push -u origin feature/back-minha-tarefa
```

Depois, abra o Pull Request no GitHub (da sua branch para `dev`).

## 5. Pull Requests

- Sempre abrir PR — sem merge direto nas branches principais (`main` e `dev` protegidas)
- Todo PR precisa de, no mínimo, **1 aprovação** antes do merge
- Resolver conflitos antes de solicitar revisão
- Garantir que a alteração foi testada
- PR deve ter descrição clara e objetiva
- Incluir a tag da área (back/front/firmware/hw/mec) e o número da issue correspondente no título do PR — PRs do tipo `docs` usam apenas o número, sem tag e sem colchetes
- Descrever no PR: o que foi feito, como testar e impacto esperado
- Caso não exista uma issue relacionada, criar a issue antes de abrir o PR

Exemplo de descrição de PR:

```text
## O que foi feito?
- Adicionada leitura dos sensores de linha.
- Criado tratamento para perda da linha.

## Como testar
- Testar com o sensor sobre a pista.
- Testar com perda temporária da linha.

## Impacto esperado
- Robô volta a seguir a linha após perda momentânea.
```

## 6. Padrão de Commits

**Estrutura:** `<tipo>: <descrição curta>`

**Tipos:**

- `feat` → nova funcionalidade
- `fix` → correção de bug
- `refactor` → refatoração sem mudança funcional
- `chore` → ajustes técnicos, configuração, dependências
- `test` → criação ou ajuste de testes
- `docs` → documentação

**Exemplos:**

- `feat: adicionar login de usuário`
- `fix: corrigir timeout na chamada de API`
- `refactor: reorganizar service de autenticação`
- `test: adicionar testes do fluxo de login`
- `docs: documentar padrão de branches e PRs`
- `chore: ajustar pipeline de build`

### Boas práticas

- Escreva o commit de forma curta e clara.
- Use verbo no presente/infinitivo de forma consistente: `adiciona` / `adicionar`, `corrige` / `corrigir`.
- Evitar mensagens genéricas como: *ajustes*, *fix bug*, *update*, *alterações*, *agora vai*.
- Preferir commits pequenos e sem misturar assuntos distintos.
- Commits não utilizam prefixo/código — apenas o tipo e a descrição.
- Não faça um commit gigante com todas as alterações do projeto.
- Não é necessário fazer um commit a cada linha alterada; faça quando concluir uma pequena unidade de trabalho.

## 7. Padrão de Nomes de PR

**Estrutura:** `[<TAG>-<NÚMERO DA ISSUE>] <tipo>: <descrição objetiva>`

O número é o número da issue no GitHub (numeração única e sequencial do repositório, não por área) — a tag apenas indica a qual área a issue pertence. PRs do tipo `docs` não usam tag nem colchetes, apenas o número da issue.

**Exemplos:**

- `[BACK-12] feat: adicionar login de usuário`
- `[FRONT-5] fix: corrigir layout do dashboard`
- `[FIRMWARE-8] refactor: otimizar leitura do sensor`
- `[BACK-19] bugfix: corrigir atualização de status na tela de login`
- `[HW-6] feat: especificar placa de circuito`
- `[MEC-2] fix: ajustar tolerância de encaixe da estrutura`
- `docs: atualizar guia de instalação`

**PR de promoção entre branches:**

- `dev -> main`
- `main -> dev`

## 8. Deploy da Documentação

- A documentação do repositório é publicada via **GitHub Pages**, a partir da branch `gh-pages`
- O deploy é **automatizado por GitHub Actions**: a cada merge na `main`, a Action gera e publica a documentação atualizada na `gh-pages`
- Não é necessário (nem recomendado) editar a `gh-pages` manualmente

## 9. Estratégia Atual do Projeto

Enquanto o projeto não possui uma versão estável entregue:

- **main** é a branch principal e representa homologação
- **dev** é utilizada para testes integrados
- Novas implementações e correções saem da main
- **Fluxo:** branch criada da main → merge em dev → promoção de dev para main

## 10. Estratégia Futura

Após a estabilização da versão final:

- **main** deverá refletir a versão entregue/estável
- **dev** deverá concentrar o desenvolvimento contínuo
- Promoções para main em pacotes validados
- Hotfixes com fluxo controlado a partir da main

## 11. Boas Práticas Gerais

- Branches curtas e objetivas
- PRs pequenos e fáceis de revisar
- Não misturar refatoração com correção funcional no mesmo PR
- Atualizar a branch com frequência para reduzir conflitos
- Validar localmente antes de abrir PR
- Nomes padronizados para facilitar buscas no histórico
- Manter rastreabilidade entre código e a área do projeto (back/front/firmware/hw/mec/docs)
- Não altere o trabalho de outra pessoa sem conversar com ela
- Revise PRs com respeito; comentários devem explicar o problema e, quando possível, sugerir uma solução
- Se houver conflito entre alterações, conversem antes de resolver

## 12. Regra para quem está aprendendo Git

**Não tenha medo de perguntar.** Git pode parecer complicado no começo.

Se aparecer um erro ou você não souber qual comando utilizar, peça ajuda antes de executar comandos que possam apagar ou sobrescrever trabalho de outras pessoas.

Comandos como `git reset --hard`, `git push --force` e exclusões de branches não devem ser utilizados sem orientação de alguém do grupo que saiba exatamente o que está fazendo.

## 13. Resumo Executivo

- **main** = principal / versão estável
- **dev** = testes e validação
- **gh-pages** = deploy automatizado da documentação via GitHub Actions (não entra no fluxo main → dev → branch)
- **Fluxo:** criar branch da main → PR para dev (mín. 1 aprovação) → validar → promover dev para main (mín. 1 aprovação)
- **Branches:** `feature/*` | `fix/*` | `refactor/*` | `bugfix/*` — sempre com tag `back`, `front`, `firmware`, `hw`, `mec` ou `docs`
- **Commit:** `tipo: descrição` (sem código/prefixo)
- **PR:** `[TAG-NÚMERO DA ISSUE] tipo: descrição`
