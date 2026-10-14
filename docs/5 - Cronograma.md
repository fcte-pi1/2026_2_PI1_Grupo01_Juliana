## Cronograma do Subsistema de Energia

<font size="3"><p style="text-align: center">Tabela 1: Cronograma do subsistema de Energia</p></font>

| ID | Tarefa | Responsável | Predecessor | Data de Início | Data de Conclusão | % de Execução | Status | Prioridade |
| :--- | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **1** | **Planejamento** | João Marcos, Giovana | — | 02/09/2026 | 30/09/2026 | 90% | Em andamento | Alta |
| 1.1 | Termo de Abertura do Projeto (contribuição de Energia no orçamento e no escopo) | João Marcos, Giovana | — | 02/09/2026 | 09/09/2026 | 100% | Concluído | Alta |
| 1.2 | Definição de Requisitos (Energia: RF01 a RF07 e RNF01 a RNF03) | João Marcos, Giovana | — | 02/09/2026 | 14/09/2026 | 100% | Concluído | Alta |
| 1.3 | Definição da Estrutura Analítica do Produto (EAP de Energia, itens 2.1 a 2.6) | João Marcos, Giovana | 1.2 | 15/09/2026 | 16/09/2026 | 100% | Concluído | Alta |
| 1.4 | Definição do Projeto Conceitual do Produto (4.2 Projeto conceitual de energia, v1.2, issue #24) | João Marcos, Giovana | 1.3 | 17/09/2026 | 28/09/2026 | 100% | Concluído | Alta |
| 1.5 | Definição do Cronograma (Energia) | João Marcos, Giovana | 1.4 | 29/09/2026 | 30/09/2026 | 90% | Em andamento | Alta |
| 1.6 | Definição do Orçamento (Energia) | João Marcos, Giovana | 1.4 | 29/09/2026 | 30/09/2026 | 50% | Em andamento | Alta |
| **2** | **Execução** | João Marcos, Giovana | 1 | 30/09/2026 | 02/12/2026 | 0% | Não iniciado | Alta |
| 2.1 | Aquisição de Partes | João Marcos, Giovana | 1.6 | 30/09/2026 | 13/10/2026 | 0% | Não iniciado | Alta |
| 2.1.1 | Alinhar com Eletrônica e Estruturas (divisor do ADC, proteção de polaridade, ponte H, alojamento da fonte) | João Marcos, Giovana | 1.4 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.1.2 | Cotar itens e fechar a lista de compras | João Marcos, Giovana | 2.1.1 | 05/10/2026 | 07/10/2026 | 0% | Não iniciado | Alta |
| 2.1.3 | Comprar itens (LiPo, carregador, buck-boost, LDO, proteções e conectores) | João Marcos, Giovana | 2.1.2 | 07/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 2.1.4 | Receber e conferir os itens | João Marcos, Giovana | 2.1.3 | 09/10/2026 | 13/10/2026 | 0% | Não iniciado | Alta |
| 2.2 | Montagem de Subsistemas | João Marcos, Giovana | 1.4 | 30/09/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 2.2.1 | Esquematizar e simular o circuito de alimentação (buck-boost, LDO e proteções) | João Marcos, Giovana | 1.4 | 30/09/2026 | 12/10/2026 | 0% | Não iniciado | Média |
| 2.2.2 | Montar o módulo de energia em protoboard (fonte, seleção AA/LiPo, proteções, buck-boost e LDO) e validar com carga simulada | João Marcos, Giovana | 2.1.4, 2.2.1 | 14/10/2026 | 19/10/2026 | 0% | Não iniciado | Alta |
| 2.2.3 | Montar o divisor do ADC com filtro e calibrar em dois pontos contra multímetro | João Marcos, Giovana | 2.2.2 | 19/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 2.3 | Testes de Subsistemas | João Marcos, Giovana | 2.2 | 21/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 2.3.1 | Testes de energia em bancada (autonomia, corrente média e de pico, corte em 7,0 V, seleção AA/LiPo) | João Marcos, Giovana | 2.2.3 | 21/10/2026 | 23/10/2026 | 0% | Não iniciado | Alta |
| 2.3.2 | Testes com a ponte H e os motores N20 da Eletrônica (linha de 5 V dentro de ±5% e sem reinício do ESP32) | João Marcos, Giovana | 2.3.1 | 23/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 2.4 | Integração de Subsistemas | João Marcos, Giovana | 2.3 | 27/10/2026 | 18/11/2026 | 0% | Não iniciado | Alta |
| 2.4.1 | Integrar a fonte à placa da Eletrônica e fixá-la no chassi | João Marcos, Giovana | 2.3.2 | 27/10/2026 | 18/11/2026 | 0% | Não iniciado | Alta |
| 2.5 | Testes de Integração | João Marcos, Giovana | 2.4 | 19/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 2.5.1 | Testes de integração com a bateria em operação real (três tentativas de 10 min) | João Marcos, Giovana | 2.4.1 | 19/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 2.6 | Apresentação Final | João Marcos, Giovana | 2.5 | 24/11/2026 | 02/12/2026 | 0% | Não iniciado | Alta |
| **3** | **Documentação** | João Marcos, Giovana | — | 30/09/2026 | 04/12/2026 | 0% | Não iniciado | Alta |
| 3.1 | Termo de Abertura do Projeto | João Marcos, Giovana | 1.1 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Média |
| 3.1.1 | Atualizar o orçamento do TAP com os itens de Energia, incluindo a bateria LiPo, que hoje não consta | João Marcos, Giovana | 1.6 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Média |
| 3.2 | Requisitos | João Marcos, Giovana | 1.2 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Média |
| 3.2.1 | Revisar os requisitos de Energia diante do conceitual v1.2 (corrente de pico, autonomia e proteções) | João Marcos, Giovana | 1.4 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Média |
| 3.3 | Estrutura Analítica do Produto | João Marcos, Giovana | 1.3 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Alta |
| 3.3.1 | Corrigir a EAP de Energia (itens 2.1 a 2.6, Figuras 1 e 3 e histórico de versões), alinhando a capacidade e a corrente de pico ao conceitual v1.2 | João Marcos, Giovana | 1.4 | 30/09/2026 | 06/10/2026 | 0% | Não iniciado | Alta |
| 3.4 | Projeto Conceitual do Produto | João Marcos, Giovana | 1.4 | 30/09/2026 | 03/11/2026 | 50% | Em andamento | Alta |
| 3.4.1 | Corrigir o 4.2 (v1.3): alerta acima do corte, limite de PWM pela tensão medida, PTC com corrente de hold e trip, diagrama de blocos (issue #160) | João Marcos, Giovana | 1.4 | 30/09/2026 | 05/10/2026 | 100% | Concluído | Alta |
| 3.4.2 | Atualizar o 4.2 (v1.4) trocando as premissas por valores medidos | João Marcos, Giovana | 2.3.2 | 27/10/2026 | 03/11/2026 | 0% | Não iniciado | Média |
| 3.5 | Cronograma | João Marcos, Giovana | 1.5 | 30/09/2026 | 04/12/2026 | 0% | Não iniciado | Média |
| 3.5.1 | Atualizar o cronograma com o realizado (contínuo) | João Marcos, Giovana | 1.5 | 30/09/2026 | 04/12/2026 | 0% | Não iniciado | Média |
| 3.6 | Orçamento | João Marcos, Giovana | 1.6 | 30/09/2026 | 04/12/2026 | 0% | Não iniciado | Média |
| 3.6.1 | Atualizar o orçamento com os valores realizados (contínuo) | João Marcos, Giovana | 2.1.4 | 13/10/2026 | 04/12/2026 | 0% | Não iniciado | Média |
| 3.7 | Testes | João Marcos, Giovana | 1.4 | 30/09/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 3.7.1 | Preencher a página 7.2 com os testes planejados e os resultados esperados, amarrados aos requisitos | João Marcos, Giovana | 1.4 | 30/09/2026 | 07/10/2026 | 0% | Não iniciado | Alta |
| 3.7.2 | Registrar os resultados obtidos na página 7.2 | João Marcos, Giovana | 2.3.1 | 23/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 3.8 | Avaliação de Desempenho | João Marcos, Giovana | 2.5 | 24/11/2026 | 02/12/2026 | 0% | Não iniciado | Média |
| 3.8.1 | Comparar o consumo medido com o estimado no conceitual e avaliar o desempenho da fonte nas três tentativas | João Marcos, Giovana | 2.5.1 | 24/11/2026 | 02/12/2026 | 0% | Não iniciado | Média |
| 3.9 | Documento Final | João Marcos, Giovana | 2.5 | 24/11/2026 | 04/12/2026 | 0% | Não iniciado | Média |
| 3.9.1 | Relatório de encerramento do projeto (parte de Energia) | João Marcos, Giovana | 2.5.1 | 24/11/2026 | 04/12/2026 | 0% | Não iniciado | Média |

<font size="2"><p style="text-align: center">Fonte: [João Marcos](https://github.com/JJOAOMARCOSS) e [Giovana Martins](https://github.com/Giih-martins), 2026.</p></font>

---

## Cronograma do Subsistema de Eletrônica

<font size="3"><p style="text-align: center">Tabela 2: Cronograma do subsistema de Eletrônica</p></font>

| ID | Tarefa | Responsável | Predecessor | Data de Início | Data de Conclusão | % de Execução | Status | Prioridade |
| :--- | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **1** | **Planejamento** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | — | 02/09/2026 | 30/09/2026 | 100% | Concluída | Alta |
| 1.1 | Termo de Abertura do Projeto (1 - TAP, contribuição de Eletrônica) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | — | 06/09/2026 | 09/09/2026 | 100% | Concluído | Alta |
| 1.2 | Definição de Requisitos (2 - Requisitos, contribuição de Eletrônica) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.1 | 10/09/2026 | 14/09/2026 | 100% | Concluído | Alta |
| 1.3 | Definição da Estrutura Analítica do Produto (3 - EAP, contribuição de Eletrônica) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.2 | 14/09/2026 | 16/09/2026 | 100% | Concluído | Alta |
| 1.4 | Definição do Projeto Conceitual do Produto (4.3 - Projeto conceitual de hardware) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.3 | 24/09/2026 | 28/09/2026 | 90% | Em andamento | Alta |
| 1.5 | Definição do Cronograma (Eletrônica) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.3 | 28/09/2026 | 30/09/2026 | 100% | Concluído | Alta |
| 1.6 | Definição do Orçamento (Eletrônica) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.3 | 28/09/2026 | 30/09/2026 | 100% | Concluído | Alta |
| **2** | **Bloco 0 (B0) — Fundação (Conceitual): fechamento da Arquitetura V2 (ToF, N20, Ponte H), pinagem e orçamento de corrente. Marco: AP7 (05/10)** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 1.3 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.1 | ELE-01: Aprovação e registro da mudança para arquitetura V2 (N20 + ToF) | Ana, João Vitor | 1.3 | 30/09/2026 | 01/10/2026 | 0% | Não iniciado | Alta |
| 2.2 | ELE-02: Fechamento e repasse da tabela de interfaces (pinagem, I²C e interrupções) para o Software | Ana, João Vitor, Marcus, Fábio | 2.1 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.3 | ELE-03: Entrega do Projeto Conceitual atualizado (AP7) | Ana, João Vitor | 1.4, 2.2 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| **3** | **Bloco 1 (B1) — Projeto e Roteamento: esquemáticos finais, roteamento da PCB, compra de componentes e prototipagem em protoboard. Marco: pronto em 22/10** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 2 | 06/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 3.1 | ELE-04: Atualização do esquemático de potência (Ponte H TB6612FNG e conectores N20) | Juan, Anderson | 2.3 | 06/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 3.2 | ELE-05: Prototipagem em protoboard do circuito Buck-Boost e da Ponte H | Juan, Anderson | 3.1 | 06/10/2026 | 14/10/2026 | 0% | Não iniciado | Alta |
| 3.3 | ELE-06: Configuração do microcontrolador, rotinas de watchdog e testes base do barramento I²C | Marcus, Fábio | 2.2, 2.3 | 06/10/2026 | 16/10/2026 | 0% | Não iniciado | Alta |
| 3.4 | ELE-07: Roteamento unificado da Placa de Circuito Impresso (PCB) / Placa Perfurada | Ana, João Vitor | 3.1, 3.2, 3.3 | 06/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| **4** | **Bloco 2 (B2) — Testes de Subsistema: medição de consumo, teste de alimentação (Buck-Boost) e resposta dos sensores I²C isolados. Marco: AP12 (26/10)** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 3 | 22/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 4.1 | ELE-08: Validação da tensão de 5V contínua pelo conversor Buck-Boost sob carga simulada | Juan, Anderson | 3.2 | 22/10/2026 | 23/10/2026 | 0% | Não iniciado | Alta |
| 4.2 | ELE-09: Teste de resposta e calibração elétrica dos 3 sensores ToF via barramento I²C | Marcus, Fábio | 3.3 | 22/10/2026 | 25/10/2026 | 0% | Não iniciado | Alta |
| 4.3 | ELE-10: Documentação das evidências de teste de hardware (AP12) | Ana, João Vitor | 4.1, 4.2 | 22/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| **5** | **Bloco 3 (B3) — Integração no Robô: soldagem da placa, montagem física no chassi (30/10), calibração de sensores e ponte H com a equipe de Software. Marco: pronto em 19/11** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 4 | 27/10/2026 | 18/11/2026 | 0% | Não iniciado | Alta |
| 5.1 | ELE-11: Soldagem dos circuitos de controle, sensores e IHM na placa final | Marcus, Fábio | 3.4, 4.2 | 27/10/2026 | 29/10/2026 | 0% | Não iniciado | Alta |
| 5.2 | ELE-12: Soldagem da etapa de potência e integração mecânica dos motores N20 e bateria | Juan, Anderson | 3.4, 4.1 | 27/10/2026 | 30/10/2026 | 0% | Não iniciado | Alta |
| 5.3 | ELE-13: Teste de bancada conjunto com Software para validar drivers, encoders reais e calibração | Juan, Anderson, Marcus, Fábio | 5.1, 5.2 | 27/10/2026 | 04/11/2026 | 0% | Não iniciado | Alta |
| 5.4 | ELE-14: Acompanhamento da 1ª navegação no labirinto 4x4 (ajuste fino de ruído elétrico) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 5.3 | 27/10/2026 | 09/11/2026 | 0% | Não iniciado | Alta |
| 5.5 | ELE-15: Verificação de robustez (vibração e curtos) para o Roteiro de Integração | Ana, João Vitor | 5.4 | 27/10/2026 | 16/11/2026 | 0% | Não iniciado | Alta |
| **6** | **Bloco 4 (B4) — Testes de Integração: testes práticos no labirinto com Estrutura, Energia, Software e Eletrônica. Marco: AP18 (23/11)** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 5 | 19/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 6.1 | ELE-16: Execução dos testes de integração completos e suporte a falhas elétricas (AP18) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio (líder: Anderson) | 5.5 | 19/11/2026 | 22/11/2026 | 0% | Não iniciado | Alta |
| **7** | **Bloco 5 (B5) — Desempenho e Entrega: reparos emergenciais, congelamento de hardware, ensaio e documentação final. Marcos: APT (02/12) e AP20 (04/12)** | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 6 | 24/11/2026 | 04/12/2026 | 0% | Não iniciado | Alta |
| 7.1 | ELE-17: Congelamento de hardware v1.0 (bloqueio de alterações físicas) | Ana, João Vitor | 6.1 | 24/11/2026 | 27/11/2026 | 0% | Não iniciado | Alta |
| 7.2 | ELE-18: Escrita do Relatório de Encerramento (Capítulo de Eletrônica) | Ana, João Vitor | 6.1, 7.1 | 24/11/2026 | 30/11/2026 | 0% | Não iniciado | Média |
| 7.3 | ELE-19: Ensaio geral e preparação técnica do robô para apresentação oficial (AP20) | Ana, João Vitor, Juan, Anderson, Marcus, Fábio | 7.1 | 24/11/2026 | 01/12/2026 | 0% | Não iniciado | Alta |

<font size="2"><p style="text-align: center">Fonte: [Ana Victória](https://github.com/navicg), [João Vitor](https://github.com/Jauzimm), [Marcus](https://github.com/MarcusVRezende), [Anderson](https://github.com/leicamAnd), [Juan](https://github.com/IndianoDev) e [Fábio](https://github.com/fabiofonteles1), 2026.</p></font>

---

## Cronograma do Subsistema de Software

Como previsão interna das nossas atividades planejamos para que o software fique pronto para os **testes de subsistema em 22/10/2026**, para os **testes de integração em 19/11/2026** e congela a versão **v1.0 em 27/11/2026** (quatro dias antes da AP12, da AP18 e da apresentação do produto, ou no último dia útil anterior). Feriados sem trabalho planejado: 12/10, 28/10, 02/11 e 20/11. Detalhamento técnico das tarefas (escopo e critérios de aceite) permanece no repositório e nas issues `ARQ-*`, `BACK-*`, `FRONT-*`, `FIRM-*` e `QA-*` do GitHub Projects.

### Orientação por marcos da matéria

| Marco | Entrega oficial | Data-alvo interna do software |
| --- | --- | --- |
| AP6 | Cronograma + Orçamento | 30/09/2026 |
| AP12 | Testes de subsistema (7.4) | 22/10/2026 (entrega 26/10) |
| AP18 | Testes de integração (7.5) | 19/11/2026 (entrega 23/11) |
| APT | Apresentação do produto | Congelamento **27/11/2026** |
| AP20 | Desempenho + relatório | 30/11/2026 |


<font size="3"><p style="text-align: center">Tabela 3: Cronograma do subsistema de Software</p></font>

| ID | Tarefa | Responsável | Predecessor | Data de Início | Data de Conclusão | % de Execução | Status | Prioridade |
| :--- | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: |
| **1** | **Planejamento** | Amanda de Moura | — | 30/09/2026 | 05/10/2026 | 100% | Concluído | Alta |
| 1.1 | Requisitos de software (RF/RNF da EAP 4.x, backlog 4.4) | Equipe de Software | — | 02/09/2026 | 28/09/2026 | 100% | Concluído | Alta |
| 1.2 | EAP de Software (4.1–4.3) e projeto conceitual 4.4 | Equipe de Software | 1.1 | 15/09/2026 | 28/09/2026 | 100% | Concluído  | Alta |
| 1.3 | Registro de decisões de arquitetura (ARQ-03, ADRs e alinhamento TAP) | Amanda de Moura | 1.2 | 30/09/2026 | 05/10/2026 | 70% | Em andamento | Alta |
| 1.4 | Definição do cronograma de Software (AP6) | Amanda de Moura | 1.2 | 29/09/2026 | 30/09/2026 | 100% | Concluído | Alta |
| 1.5 | Definição do orçamento de Software (AP6) | Amanda de Moura | 1.2 | 29/09/2026 | 30/09/2026 | 100% | Concluído | Alta |
| 1.6 | Contratos e esqueletos (ARQ-01, ARQ-02, ARQ-04–ARQ-06) + CI e issues (ARQ-08) | Amanda de Moura, Karoline Luz, João Marcos, Chris Escobar, Luiza Pugas | 1.3 | 30/09/2026 | 05/10/2026 | 10% | Em andamento | Alta |
| **2** | **Execução** | Equipe Software | 1 | 30/09/2026 | 02/12/2026 | 5% | Em andamento | Alta |
| **2.1** | **B0 · Fundação (30/09 → 05/10)** | — | 1.6 | 30/09/2026 | 05/10/2026 | 0% | Em andamento | Alta |
| 2.1.1 | ARQ-03 · Registro de decisões e divergências | Amanda de Moura | — | 30/09/2026 | 05/10/2026 | 80% | Em andamento | Alta |
| 2.1.2 | ARQ-02 · Contrato REST + SSE (OpenAPI) | Amanda de Moura | 2.1.1 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.1.3 | ARQ-01 · Contrato de telemetria serial v1 | Chris Escobar | — | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.1.4 | ARQ-04 · Estrutura do backend (camadas, Docker) | Amanda de Moura | — | 30/09/2026 | 02/10/2026 | 0% | Não iniciado | Alta |
| 2.1.5 | ARQ-05 · Estrutura do front-end (rotas, protótipo) | Karoline Luz, João Marcos | — | 30/09/2026 | 01/10/2026 | 0% | Não iniciado | Alta |
| 2.1.6 | ARQ-06 · Esqueleto do firmware + HAL simulada | Luiza Pugas | — | 30/09/2026 | 02/10/2026 | 0% | Não iniciado | Alta |
| 2.1.7 | ARQ-09 · Interface com Eletrônica, Energia e Estrutura | João Marcos | outras áreas | 30/09/2026 | 07/10/2026 | 0% | Não iniciado | Alta |
| 2.1.8 | ARQ-07 · Simulador de robô (telemetria) | Chris Escobar | 2.1.3 | 05/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 2.1.9 | ARQ-08 · CI, definição de pronto e 46 issues no GitHub Projects | Amanda de Moura | 2.1.4, 2.1.5, 2.1.6 | 01/10/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| **2.2** | **B1 · Construção do núcleo (30/09 → 21/10 · gate 22/10)** | — | 2.1 | 30/09/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 2.2.1 | BACK-01 · Modelo de dados e repositório | Amanda de Moura | 2.1.4 | 02/10/2026 | 06/10/2026 | 0% | Não iniciado | Alta |
| 2.2.2 | FRONT-01 · Estrutura, rotas e ClienteAPI | Karoline Luz | 2.1.5 | 02/10/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.2.3 | FIRM-02 · Simulador de navegação no PC | Luiza Pugas, João Marcos | 2.1.6 | 01/10/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
| 2.2.4 | BACK-06 · Consultas (histórico, filtro, detalhe) | João Marcos | 2.1.4 | 02/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 2.2.5 | FRONT-02 · Início e Nova execução | Karoline Luz | 2.2.2, 2.1.2 | 06/10/2026 | 08/10/2026 | 0% | Não iniciado | Alta |
| 2.2.6 | BACK-02 · Ingestão e validação da telemetria | Chris Escobar | 2.2.1, 2.1.3 | 07/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 2.2.7 | FIRM-03 · Driver dos 3 ToF (HAL simulada) | Luiza Pugas, Karoline Luz | 2.1.6 | 06/10/2026 | 09/10/2026 | 0% | Não iniciado | Alta |
| 2.2.8 | BACK-03 · Gerenciador de execuções e tentativas | Amanda de Moura | 2.2.1, 2.1.1 | 07/10/2026 | 13/10/2026 | 0% | Não iniciado | Alta |
| 2.2.9 | FRONT-05 · Encerrar e Retomar tentativa | Karoline Luz | 2.2.5 | 09/10/2026 | 13/10/2026 | 0% | Não iniciado | Alta |
| 2.2.10 | FIRM-06 · Leitura da bateria (simulada) | Luiza Pugas, João Marcos | 2.1.6 | 10/10/2026 | 13/10/2026 | 0% | Não iniciado | Alta |
| 2.2.11 | FIRM-01 · Flood fill e leitura do tipo no DIP switch | Chris Escobar | 2.2.3 | 10/10/2026 | 16/10/2026 | 0% | Não iniciado | Alta |
| 2.2.12 | QA-01 · Plano de testes 7.4 (casos 4.1–4.16) | Amanda de Moura | 2.1.1 | 09/10/2026 | 16/10/2026 | 0% | Não iniciado | Alta |
| 2.2.13 | BACK-04 · Cálculo de métricas | Amanda de Moura | 2.2.6 | 13/10/2026 | 15/10/2026 | 0% | Não iniciado | Alta |
| 2.2.14 | BACK-07 · Stream SSE em tempo real | Amanda de Moura | 2.2.6 | 13/10/2026 | 15/10/2026 | 0% | Não iniciado | Alta |
| 2.2.15 | FIRM-07 · Tarefa de telemetria (serial USB) | Chris Escobar, Luiza Pugas | 2.1.3 | 09/10/2026 | 14/10/2026 | 0% | Não iniciado | Alta |
| 2.2.16 | FRONT-03 · Tela de tempo real | Karoline Luz | 2.2.2, 2.2.14 | 14/10/2026 | 16/10/2026 | 0% | Não iniciado | Alta |
| 2.2.17 | BACK-05 · Monitores de link e tempo (10 min) | João Marcos | 2.2.8 | 15/10/2026 | 16/10/2026 | 0% | Não iniciado | Alta |
| 2.2.18 | FIRM-04 · Motores, encoders e PID (simulado) | Luiza Pugas, Karoline Luz | 2.1.6 | 14/10/2026 | 20/10/2026 | 0% | Não iniciado | Alta |
| 2.2.19 | FRONT-04 · Labirinto e trajeto | João Marcos | 2.2.2 | 13/10/2026 | 19/10/2026 | 0% | Não iniciado | Média |
| 2.2.20 | BACK-08 · Ponte serial (Bluetooth real no B3) | Chris Escobar | 2.1.3, 2.2.6 | 16/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 2.2.21 | FRONT-06 · Histórico e detalhe da execução | João Marcos | 2.2.2, 2.2.4 | 16/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| 2.2.22 | FIRM-05 · Primitivas de movimento (simulado) | Luiza Pugas, João Marcos | 2.2.7, 2.2.18 | 20/10/2026 | 21/10/2026 | 0% | Não iniciado | Alta |
| **2.3** | **B2 · Testes de subsistema (22/10 → 26/10 · AP12)** | — | 2.2 | 22/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 2.3.1 | QA-02 · Executar casos 4.1–4.16, evidências e correções | Equipe Software | gate 22/10 | 22/10/2026 | 25/10/2026 | 0% | Não iniciado | Alta |
| **2.4** | **B3 · Integração no robô (27/10 → 18/11 · gate 19/11)** | — | 2.3 | 27/10/2026 | 18/11/2026 | 0% | Não iniciado | Alta |
| 2.4.1 | FIRM-08 · NVS e retomada | João Marcos | 2.2.11 | 27/10/2026 | 30/10/2026 | 0% | Não iniciado | Alta |
| 2.4.2 | FIRM-09 · Estados, health-check e BOOT | Amanda de Moura, Karoline Luz | 2.2.7, 2.2.18, 2.2.10 | 27/10/2026 | 30/10/2026 | 0% | Não iniciado | Alta |
| 2.4.3 | FIRM-11 · Bluetooth real (robô → ponte → backend) | Chris Escobar, Luiza Pugas | robô montado, 2.2.20, 2.2.15 | 27/10/2026 | 30/10/2026 | 0% | Não iniciado | Alta |
| 2.4.4 | FIRM-10 · Drivers reais e calibração | Luiza Pugas, Chris Escobar | robô montado, 2.2.7–2.2.22 | 30/10/2026 | 04/11/2026 | 0% | Não iniciado | Alta |
| 2.4.5 | FIRM-12 · Primeira navegação completa 4×4 físico | Luiza Pugas, Karoline Luz | 2.4.4 | 03/11/2026 | 09/11/2026 | 0% | Não iniciado | Alta |
| 2.4.6 | QA-05 · Falhas e retomadas ponta a ponta | Chris Escobar, Luiza Pugas, Amanda de Moura | 2.4.3, 2.4.1 | 04/11/2026 | 13/11/2026 | 0% | Não iniciado | Alta |
| 2.4.7 | FIRM-13 · Robustez (4×4 real; 8×4/12×4 simulados) | João Marcos, Luiza Pugas | 2.4.5 | 09/11/2026 | 16/11/2026 | 0% | Não iniciado | Alta |
| 2.4.8 | FRONT-07 · Should/Could restantes | Karoline Luz, João Marcos | 2.3.1 | 27/10/2026 | 16/11/2026 | 0% | Não iniciado | Média |
| 2.4.9 | QA-03 · Roteiro de testes de integração 7.5 | Amanda de Moura, Karoline Luz | outras áreas | 09/11/2026 | 16/11/2026 | 0% | Não iniciado | Alta |
| **2.5** | **B4 · Testes de integração (19/11 → 23/11 · AP18)** | — | 2.4 | 19/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 2.5.1 | QA-04 · Executar 5.1–5.16 com todas as áreas | Equipe Software | 2.4.9, gate 19/11 | 19/11/2026 | 22/11/2026 | 0% | Não iniciado | Alta |
| **2.6** | **B5 · Desempenho e apresentação (24/11 → 04/12)** | — | 2.5 | 24/11/2026 | 04/12/2026 | 0% | Não iniciado | Alta |
| 2.6.1 | QA-06 · Congelamento e tag v1.0 | Amanda de Moura | 2.5.1 | 27/11/2026 | 27/11/2026 | 0% | Não iniciado | Alta |
| 2.6.2 | BACK-09 · Coleta de desempenho e análise | Chris Escobar, Amanda de Moura | 2.6.1 | 24/11/2026 | 28/11/2026 | 0% | Não iniciado | Média |
| 2.6.3 | QA-07 · Roteiro operacional e ensaio da demo (APT) | Equipe Software | 2.6.1 | 28/11/2026 | 01/12/2026 | 0% | Não iniciado | Alta |
| 2.6.4 | Apresentação do produto (APT 02/12) | Equipe Software | 2.6.3 | 01/12/2026 | 02/12/2026 | 0% | Não iniciado | Alta |
| **3** | **Documentação** | Amanda de Moura | — | 30/09/2026 | 04/12/2026 | 20% | Em andamento | Alta |
| 3.1 | Projeto conceitual de software (4.4) — manutenção contínua | Amanda de Moura, equipe | 1.2 | 30/09/2026 | 27/11/2026 | 90% | Em andamento | Alta |
| 3.2 | Registro de decisões em `src/README.md` e ADRs | Amanda de Moura | 2.1.1 | 30/09/2026 | 05/10/2026 | 30% | Em andamento | Média |
| 3.3 | Testes de software — plano e resultados (7.4) | Amanda de Moura | 2.2.12, 2.3.1 | 09/10/2026 | 26/10/2026 | 0% | Não iniciado | Alta |
| 3.4 | Testes de integração — roteiro e resultados (7.5) | Amanda de Moura, Karoline Luz | 2.4.9, 2.5.1 | 09/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 3.5 | Cronograma de Software — atualização com o realizado | Amanda de Moura | 1.4 | 30/09/2026 | 04/12/2026 | 10% | Em andamento | Média |
| 3.6 | Orçamento de Software — atualização com o realizado | Amanda de Moura | 1.5 | 30/09/2026 | 04/12/2026 | 100% | Concluído | Média |
| 3.7 | Avaliação de desempenho (doc. 8) — dados de software | Chris Escobar, Amanda de Moura | 2.6.2 | 24/11/2026 | 02/12/2026 | 0% | Não iniciado | Média |
| 3.8 | Relatório de encerramento — seção de software (ARQ-10) | Amanda de Moura | 2.6.1 | 27/11/2026 | 30/11/2026 | 0% | Não iniciado | Média |

<font size="2"><p style="text-align: center">Fonte: [Amanda de Moura](https://github.com/AmandaaMoura), 2026. Gerência de Software — PI1 Grupo 01.</p></font>

---

## Cronograma do Subsistema de Estrutura

<font size="3"><p style="text-align: center">Tabela 4: Cronograma do subsistema de Estrutura</p></font>

| **ID** | **Tarefa** | **Responsável** | **Predecessor** | **Data de Início** | **Data de Conclusão** | **% de Execução** | **Status** | **Prioridade** |
|:------:|------------|-----------------|-----------------|--------------------|-----------------------|:-----------------:|-------------|----------------|
| 1 | **Definição Conceitual** | Estrutura | — | 16/09/2026 | 29/09/2026 | 100% | Concluído | Alta |
| 1.1 | Levantamento de requisitos dimensionais do labirinto | Jefferson | — | 16/09/2026 | 20/09/2026 | 100% | Concluído | Alta |
| 1.2 | Seleção de materiais (chassi, rodas, fixação) | Geovana | 1.1 | 21/09/2026 | 25/09/2026 | 100% | Concluído | Alta |
| 1.3 | Definição dos atuadores (motor DC N20 com encoder) | Júlio César | 1.1 | 21/09/2026 | 25/09/2026 | 100% | Concluído | Alta |
| 1.4 | Projeto conceitual de estruturas (documento) | Murilo | 1.2, 1.3 | 22/09/2026 | 29/09/2026 | 100% | Concluído | Alta |
| 2 | **Modelagem e Prototipagem** | Estrutura | 1 | 01/10/2026 | 19/10/2026 | 0% | Não iniciado | Alta |
| 2.1 | Modelagem CAD do chassi e suportes | Jefferson | 1.4 | 01/10/2026 | 08/10/2026 | 0% | Não iniciado | Alta |
| 2.2 | Modelagem CAD da torre de sensores | Geovana | 1.4 | 01/10/2026 | 08/10/2026 | 0% | Não iniciado | Alta |
| 2.3 | Corte/impressão do chassi (acrílico ou PETG) | Júlio César | 2.1 | 09/10/2026 | 15/10/2026 | 0% | Não iniciado | Alta |
| 2.4 | Impressão 3D dos suportes e torre de sensores | Murilo | 2.2 | 09/10/2026 | 15/10/2026 | 0% | Não iniciado | Alta |
| 2.5 | Aquisição de motores, rodas e *caster* | Jefferson | 1.3 | 01/10/2026 | 12/10/2026 | 0% | Não iniciado | Alta |
| 3 | **Montagem** | Estrutura | 2 | 16/10/2026 | 24/10/2026 | 0% | Não iniciado | Alta |
| 3.1 | Fixação dos motores e rodas ao chassi | Júlio César | 2.3, 2.5 | 16/10/2026 | 20/10/2026 | 0% | Não iniciado | Alta |
| 3.2 | Montagem do suporte de bateria e torre de sensores | Murilo | 2.4 | 16/10/2026 | 20/10/2026 | 0% | Não iniciado | Alta |
| 3.3 | Ajuste e alinhamento final da estrutura | Geovana | 3.1, 3.2 | 21/10/2026 | 24/10/2026 | 0% | Não iniciado | Alta |
| 4 | **Testes de Estrutura (AP12)** | Estrutura | 3 | 25/10/2026 | 25/10/2026 | 0% | Não iniciado | Alta |
| 4.1 | Teste de rigidez e fixação mecânica | Jefferson | 3.3 | 25/10/2026 | 25/10/2026 | 0% | Não iniciado | Alta |
| 4.2 | Teste de giro (90° e 180°) e repetibilidade | Geovana | 3.3 | 25/10/2026 | 25/10/2026 | 0% | Não iniciado | Alta |
| 5 | **Testes de Integração (AP18)** | Estrutura | 4 | 23/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 5.1 | Integração estrutura, eletrônica e energia | Todos (Estrutura) | 4 | 23/11/2026 | 23/11/2026 | 0% | Não iniciado | Alta |
| 6 | **Apresentação Final** | Estrutura | 5 | 02/12/2026 | 02/12/2026 | 0% | Não iniciado | Alta |

<font size="2"><p style="text-align: center">Fonte: [Jefferson de Souza Reis](https://github.com/jeffh), [Geovana](https://github.com/Bygeo57), [Júlio César](https://github.com/DeNNis715) e [Murilo](https://github.com/MuriloPi13), 2026.</p></font>

