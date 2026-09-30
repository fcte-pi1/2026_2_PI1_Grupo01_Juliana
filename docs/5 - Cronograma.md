# Cronograma

Exporte as informações do GitHub Projects [em formato CSV](https://docs.github.com/en/issues/planning-and-tracking-with-projects/managing-your-project/exporting-your-projects-data), e [renderize em Markdown](https://www.google.com/search?q=convert+CSV+file+to+Markdown+table) no formato a seguir:

| **ID** | **Tarefa** | **Responsável** | **Predecessor** | **Data de Início** | **Data de Conclusão** | **% de Execução** | **Status** | **Prioridade** |
|:------:|------------|-----------------|-----------------|--------------------|-----------------------|:-----------------:|-------------|----------------|
| 1 | **Planejamento** | | | | | | Não iniciado | |
| 1.1 | Termo de Abertura do Projeto | | | | | | Não iniciado | |
| 1.2 | Definição de Requisitos | | | | | | Não iniciado | |
| 1.3 | Definição da Estrutura Analítica do Produto | | | | | | Não iniciado | |
| 1.4 | Definição do Projeto Conceitual do Produto | | | | | | Não iniciado | |
| 1.5 | Definição do Cronograma | | | | | | Não iniciado | |
| 1.6 | Definição do Orçamento | | | | | | Não iniciado | |
| 2 | **Execução** | | | | | | Não iniciado | |
| 2.1 | Aquisição de Partes | | | | | | Não iniciado | |
| 2.2 | Montagem de Subsistemas | | | | | | Não iniciado | |
| 2.3 | Testes de Subsistemas | | | | | | Não iniciado | |
| 2.4 | Integração de Subsistemas | | | | | | Não iniciado | |
| 2.5 | Testes de Integração | | | | | | Não iniciado | |
| 2.6 | Apresentação Final | | | | | | Não iniciado | |
| 3 | **Documentação** | | | | | | Não iniciado | |
| 3.1 | Termo de Abertura do Projeto | | | | | | Não iniciado | |
| 3.2 | Requisitos | | | | | | Não iniciado | |
| 3.3 | Estrutura Analítica do Produto | | | | | | Não iniciado | |
| 3.4 | Projeto Conceitual do Produto | | | | | | Não iniciado | |
| 3.5 | Cronograma | | | | | | Não iniciado | |
| 3.6 | Orçamento | | | | | | Não iniciado | |
| 3.7 | Testes | | | | | | Não iniciado | |
| 3.8 | Avaliação de Desempenho | | | | | | Não iniciado | |
| 3.9 | Documento Final | | | | | | Não iniciado | |

---

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
| 3.4 | Projeto Conceitual do Produto | João Marcos, Giovana | 1.4 | 30/09/2026 | 03/11/2026 | 0% | Não iniciado | Alta |
| 3.4.1 | Corrigir o 4.2 (v1.3): alerta acima do corte, limite de PWM pela tensão medida, PTC com corrente de hold e trip, diagrama de blocos | João Marcos, Giovana | 1.4 | 30/09/2026 | 05/10/2026 | 0% | Não iniciado | Alta |
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
