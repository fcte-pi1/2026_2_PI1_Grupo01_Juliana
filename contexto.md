# Contexto do Projeto — PI1 Grupo 01 (Profª Juliana - 2026/2)

Este documento foi criado para ajudar você a entender **exatamente o que é o projeto**, qual é a **estrutura do repositório**, como funciona o **fluxo de trabalho com Git/Branches**, o papel de cada **arquivo `.md`** e um **guia prático do que você precisa aprender** para se situar na matéria.

---

## 1. Visão Geral do Projeto

* **Disciplina:** Projeto Integralizador 1 (PI1) — UnB / FCTE
* **Turma / Grupo:** Semestre 2026/2 — Grupo 01 (Professora Juliana)
* **Objetivo do Projeto:** Desenvolvimento de um **robô móvel autônomo (*Micromouse*)** capaz de navegar e resolver labirintos com precisão, contornando limitações sensoriais e de processamento embarcado.
* **Diferencial / Sistema Integrado:** O projeto inclui um **sistema de telemetria sem fio** em tempo real com persistência de dados para monitorar o estado do robô (leitura de sensores, estimativa de posição, velocidade e consumo de bateria) e exibição em um dashboard web.

---

## 2. Estrutura do Repositório e Subsistemas

O projeto é multidisciplinar e está dividido em 5 frentes principais, refletidas na estrutura de pastas:

```text
2026_2_PI1_Grupo01_Juliana/
├── .github/              # Automações de CI/CD (ex: deploy automatizado do MkDocs)
├── docs/                 # Documentação técnica completa em Markdown (compilada com MkDocs)
├── hw/                   # Hardware (esquemáticos, simulações, KiCad, datasheets, diagramas de blocos)
├── mec/                  # Mecânica e Estruturas (arquivos CAD, STL 3D, desenhos técnicos, simulações)
├── src/                  # Código-fonte do sistema de software
│   ├── firmware/         # Código embarcado do microcontrolador (ESP32, Arduino, C/C++/Python)
│   ├── backend/          # API REST e Banco de dados (Python/Flask/FastAPI ou Node/Express)
│   └── frontend/         # Interface / Dashboard web (React, Vue ou HTML/CSS/JS)
├── CONTRIBUTING.md       # Regras e convenções para uso do Git, branches, commits e Pull Requests
├── mkdocs.yml            # Arquivo de configuração do gerador de site da documentação
└── README.md             # Visão geral do repositório do grupo
```

---

## 3. Entendendo as Branches e o Fluxo de Trabalho Git

De acordo com o documento `CONTRIBUTING.md`, existe uma política rigorosa de branches no projeto:

### 3.1 As Branches Principais
* **`main`**: Branch principal que reflete a versão estável/homologada do projeto. **Ninguém comita diretamente nela**.
* **`dev`**: Branch de integração, testes e validação das funcionalidades antes de subir para a `main`. **Ninguém comita diretamente nela**.
* **`gh-pages`**: Branch gerenciada automaticamente por GitHub Actions para publicar a documentação em HTML via GitHub Pages.

### 3.2 O Fluxo de Desenvolvimento
```text
main  ──>  criar branch de trabalho  ──>  commits  ──>  PR  ──>  dev  ──>  testes/validação  ──>  PR  ──>  main
```

### 3.3 Padrão de Nomenclatura das Branches
Toda branch é criada a partir da `main` e deve seguir o padrão:

`<tipo>/<tag>-<descrição-curta>`

* **Tipos aceitos:** `feature/` (nova funcionalidade), `fix/` (correção pontual), `refactor/` (melhoria interna), `bugfix/` (correção de bug).
* **Tags por área:** `back` | `front` | `firmware` | `hw` | `mec` | `docs`

**Exemplos reais de branches:**
* `feature/back-login`
* `feature/firmware-leitura-sensores`
* `fix/front-ajuste-layout`
* `feature/docs-atualizar-tap`
* `feature/mec-suporte-bateria`
* `feature/hw-esquematico-alimentacao`

### 3.4 Padrão de Commits e Pull Requests (PR)
* **Commits:** Usar Conventional Commits: `<tipo>: <descrição>`
  * Exemplo: `feat: adicionar leitura dos sensores de linha`
  * Exemplo: `docs: atualizar requisitos funcionais no arquivo .md`
* **Pull Requests (PR):** Devem referenciar a issue e a área:
  * Padrão: `[<TAG>-<NÚMERO DA ISSUE>] <tipo>: <descrição>`
  * Exemplo: `[FIRMWARE-8] feat: implementar algoritmo de busca no labirinto`
  * PRs de documentação: `docs: atualizar guia de instalação` (sem tag).

---

## 4. Mapeamento da Documentação (`docs/*.md`)

A documentação acadêmica e técnica do projeto está inteiramente contida na pasta `docs/` em arquivos Markdown (`.md`). Cada arquivo possui um propósito específico no ciclo de vida de PI1:

| Arquivo `.md` | Nome do Documento | Para que serve / O que deve conter |
|---|---|---|
| `1 - TAP.md` | Termo de Abertura do Projeto | Justificativa, problema, objetivos, escopo inicial, integrantes, orientação, orçamento e riscos. |
| `2 - Requisitos.md` | Tabela de Requisitos | Requisitos funcionais (RF) e não-funcionais (RNF) do robô e do software com prioridades. |
| `3 - EAP.md` | Estrutura Analítica do Produto | Decomposição hierárquica do produto nos 4 subsistemas (Estrutura, Energia, Hardware e Software). |
| `4.1 - ...estrutura.md` | Projeto Conceitual de Estruturas | Especificações mecânicas, materiais, chassis, atuadores e desenho estrutural. |
| `4.2 - ...energia.md` | Projeto Conceitual de Energia | Bateria, circuito de alimentação, reguladores de tensão, estimativa de consumo e autonomia. |
| `4.3 - ...hardware.md` | Projeto Conceitual de Hardware | Diagrama de blocos, esquemático elétrico dos circuitos, pinagem, sensores e atuadores. |
| `4.4 - ...software.md` | Projeto Conceitual de Software | Arquitetura de software, diagramas BPMN/UML, modelo ER de banco de dados, requisitos MoSCoW e protótipos. |
| `5 - Cronograma.md` | Cronograma do Projeto | Datas de entregas, marcos (milestones) e distribuição de tarefas ao longo do semestre. |
| `6 - Orçamento.md` | Orçamento do Projeto | Custo estimado de componentes, ferramentas, manufatura e insumos. |
| `7.1` a `7.5 - Testes...md` | Planos e Relatórios de Testes | Procedimentos de teste para Estrutura, Energia, Hardware, Software e Integração completa do Micromouse. |
| `8 - Avaliação...md` | Avaliação de Desempenho | Métricas reais obtidas nos testes (tempo no labirinto, precisão, autonomia, latência da telemetria). |
| `9 - Relatório...md` | Relatório de Encerramento | Conclusões finais, lições aprendidas e encerramento oficial do projeto PI1. |

---

## 5. Guia Prático: O que você precisa aprender para se destravar

Se você está se sentindo perdido na disciplina PI1, siga este passo a passo ordenado para se atualizar rapidamente:

### Passo 1: Domine o básico de Git e GitHub (Essencial)
1. Entenda os comandos básicos:
   * `git switch main` e `git pull origin main` (para sempre atualizar sua base).
   * `git switch -c feature/<sua-area>-<sua-tarefa>` (para criar sua branch de trabalho).
   * `git add .`, `git commit -m "feat: descrição"` e `git push -u origin <sua-branch>`.
2. Saiba abrir um **Pull Request (PR)** da sua branch para a branch `dev` no GitHub.
3. **Regra de Ouro:** Nunca faça `git push` direto na `main` ou na `dev`.

### Passo 2: Entenda o Robô Micromouse e o Projeto
1. **O que o robô faz?** Navega em um labirinto, detecta paredes usando sensores (ex: infravermelho/ultrassônico), calcula rotas e transmite telemetria via rádio/Wi-Fi/Bluetooth.
2. **O que o software faz?** Recebe os dados de telemetria enviados pelo robô, grava em um banco de dados e exibe em uma tela (dashboard) os gráficos de velocidade, bateria e mapa.

### Passo 3: Identifique a sua Área de Atuação no Grupo
Pergunte ou defina com seu grupo em qual pasta você irá colaborar com mais frequência:
* **Mecânica (`mec/`)**: Se você for trabalhar com modelagem 3D, CAD (Fusion 360/FreeCAD), impressão 3D ou cálculo estrutural.
* **Hardware (`hw/`)**: Se você for desenhar esquemáticos (KiCad), montar protoboard/placa, escolher sensores ou fontes de alimentação.
* **Firmware (`src/firmware/`)**: Se você for programar o microcontrolador (ESP32/Arduino em C++ ou Python) para ler sensores e controlar motores.
* **Backend (`src/backend/`)**: Se você for criar a API de telemetria e o banco de dados (Python/Node.js).
* **Frontend (`src/frontend/`)**: Se você for construir as telas do dashboard de monitoramento.
* **Documentação (`docs/`)**: Todos os membros devem contribuir preenchendo os arquivos `.md` referentes às suas tarefas!

### Passo 4: Como escrever em Markdown (`.md`)
Os arquivos `.md` são textos formatados de maneira simples. Aprenda os marcadores básicos:
* `# Título Principal`, `## Subtítulo`, `### Seção`
* `**Negrito**`, `*Itálico*`
* `- Item de lista` ou `1. Item numerado`
* `[Texto do link](http://link.com)`
* `| Tabela | Coluna |`

---

## 6. Próximos Passos Sugeridos

1. Read a documentação existente em `docs/1 - TAP.md` e `docs/2 - Requisitos.md`.
2. Alinhe com seu grupo qual Issue/tarefa está atribuída a você.
3. Crie sua primeira branch seguindo o padrão (ex: `feature/docs-preencher-tap`).
4. Edite o arquivo correspondente e abra seu primeiro Pull Request para `dev`.
