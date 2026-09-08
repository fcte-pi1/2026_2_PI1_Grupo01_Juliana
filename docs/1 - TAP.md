# Termo de Abertura do Projeto

- **Nome do Projeto:**
- **Data de Início: 02/09/2026**
- **Data de Término: 12/12/2026**

## Visão Geral do Projeto

### Descrição do Problema

O projeto aborda o desafio de desenvolver um robô móvel autônomo (*Micromouse*) capaz de navegar e resolver labirintos com precisão, contornando limitações sensoriais e de processamento embarcado. Além disso, busca resolver a falta de visibilidade em tempo real dos estados do robô (leitura de sensores, estimativa de posição e consumo de bateria) através da integração de um sistema de telemetria sem fio e persistência de dados.

### Objetivos

Insira o texto aqui

### Escopo do Projeto

O projeto compreende o desenvolvimento, a integração, os testes e a documentação de um sistema Micromouse autônomo capaz de solucionar os labirintos estabelecidos pela disciplina e disponibilizar os dados de telemetria exigidos.

#### Escopo do Produto

- Estrutura física do Micromouse, que não pode exceder as dimensões de 16,5 cm de comprimento ou largura.
- Sistema eletrônico/hardware dedicado, englobando a placa de circuito impresso customizada, microcontrolador, sensoriamento do labirinto e acionamento mecânico.
- Sistema de energia para alimentação autônoma e segura dos componentes embarcados.
- Software embarcado para operação 100% autônoma, contemplando o mapeamento, descoberta de paredes, localização e navegação.
- Sistema web de telemetria em tempo real, capaz de exibir o tipo do labirinto, trajeto, consumo de bateria, velocidade média, tempo de conclusão e status do desafio.
- Banco de dados para armazenamento do histórico de execuções, permitindo consultas específicas ou gerais de todos os labirintos.

#### Atividades Incluídas no Escopo

- Levantamento de requisitos e desenvolvimento do projeto conceitual das quatro frentes da engenharia envolvidas (Estrutura, Energia, Hardware e Software).
- Integração física e lógica entre todos os subsistemas do robô.
- Construção de uma pista simplificada de dimensões 4x4 para a realização de testes análogos aos da competição.
- Execução de testes isolados de cada subsistema e de integração, com análise e correção de falhas.
- Gerenciamento contínuo das atividades e versionamento de código, esquemáticos e documentação utilizando as ferramentas GitHub e GitHub Projects, com issues limitadas a até dois responsáveis.

#### Fora de Escopo

- **Aplicativos Móveis Nativos:** O desenvolvimento de aplicativos dedicados para Android ou iOS para visualização da telemetria (o escopo restringe-se ao sistema Web).
- **Fabricação das Pistas em Escala Maior:** A construção física das pistas em tamanho 8x4 e 12x4 para testes da equipe.
- **Base de Recarga Autônoma:** O robô não possuirá sistema de retorno automático a uma base (dock) de carregamento; o gerenciamento da bateria será feito de forma manual.
- **Mapeamento LiDAR ou Visão Computacional Avançada:** O uso de sensores de escaneamento a laser complexos ou processamento de imagem por câmeras para o mapeamento do labirinto.
- **Operação Teleoperada:** Qualquer tipo de controle remoto ou manual do trajeto do robô.

#### Critérios de Aceite

- O Micromouse deve partir de um canto do labirinto e encontrar de forma autônoma a área de objetivo localizada no canto diametralmente oposto.
- O robô deve ser capaz de resolver com sucesso os três modelos propostos de labirinto: 4x4 (72x72 cm²), 8x4 (144x72 cm²) e 12x4 (216x72 cm²).
- Os dados telemétricos devem ser obrigatoriamente enviados e exibidos no sistema web; o não cumprimento desta funcionalidade penaliza a nota de avaliação do desafio em 25%.


### _Stakeholders_

*Equipe de desenvolvimento (Grupo 01):* responsável pelo projeto, desenvolvimento e testes do micromouse e do sistema de telemetria/banco de dados. Interesse direto na aprovação e no bom desempenho do produto.

*Professores da disciplina PI1:* acompanham o desenvolvimento do projeto, avaliam as entregas ao longo do semestre e atuam como banca nos testes de integração e na apresentação final.

## Recursos do Projeto

### Membros da Equipe

| **Nome** | **Matrícula** | **Curso** | **E-mail** | **Funções** |
|----------|---------------|-----------|------------|-------------|
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |
| | | | | |

**Orientador:**

### Orçamento estimado (R$)

Discutam dentro da equipe a verba possível disponível para o desenvolvimento do projeto, com base na complexidade do projeto, na quantidade de membros e na realidade de cada um.

### Esforço estimado (horas)

Estimem com sinceridade o tempo a ser despendido no desenvolvimento do projeto, com base na complexidade do projeto, na quantidade de membros e na realidade de cada um.
