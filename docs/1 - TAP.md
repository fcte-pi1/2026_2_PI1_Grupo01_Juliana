# Termo de Abertura do Projeto
> Termo de abertura do projeto / Project Charter. Um documento publicado pelo iniciador ou patrocinador do projeto que autoriza formalmente a existência de um projeto e fornece ao gerente do projeto a autoridade para aplicar os recursos organizacionais nas atividades do projeto.

- **Nome do Projeto:**
- **Data de Início: 02/09/2026**
- **Data de Término: 12/12/2026**

## Visão Geral do Projeto

### Descrição do Problema

O projeto consiste na construção de um *micromouse*: um robô autônomo que precisa percorrer um labirinto desconhecido, sem controle externo, até encontrar a saída no menor tempo possível. Para isso, o robô precisa ler o ambiente em tempo real com seus sensores, decidir sozinho qual caminho seguir e mover-se com a estrutura, a energia e o hardware embarcado funcionando de forma coordenada.

Só construir o robô, porém, não é suficiente para entender o que ele está fazendo durante os testes: é difícil saber, por exemplo, por que ele errou uma curva ou qual sensor falhou olhando só para o percurso físico. Por isso o projeto inclui também um sistema de telemetria, que coleta os dados de navegação do robô durante os testes e os grava em um banco de dados, permitindo à equipe analisar o comportamento do micromouse depois de cada tentativa.

### Objetivos

O objetivo do projeto é desenvolver um micromouse autônomo — capaz de mapear e resolver um labirinto sem intervenção externa — integrado a um sistema de telemetria que registre os dados de navegação em um banco de dados, permitindo à equipe consultar e analisar o desempenho do robô após cada teste.

- **Específico:** projetar e construir um micromouse composto por estrutura mecânica, fonte de energia, hardware de sensoriamento/atuação e firmware de navegação, capaz de percorrer um labirinto até a saída, com um sistema de telemetria que registre os dados de cada percurso em um banco de dados.
- **Mensurável:** o robô deve completar o percurso do labirinto proposto, e o sistema de telemetria deve registrar corretamente os dados de navegação (como trajetória, tempo de percurso e leituras de sensores) de cada tentativa.
- **Acordado:** definido e validado entre a equipe de desenvolvimento (frentes de estrutura, energia, hardware, firmware e software) e os professores da disciplina PI1, responsáveis por avaliar as entregas.
- **Realista:** compatível com o orçamento e os componentes disponíveis para a equipe, bem como com o tempo e a carga horária previstos para o semestre.
- **Limitado no tempo:** protótipo funcional e documentação do projeto concluídos até 12/12/2026, data de término definida para o projeto.

### Escopo do Projeto

Insira o texto aqui

### _Stakeholders_

*Equipe de desenvolvimento (Grupo 01):* responsável pelo projeto, desenvolvimento e testes do micromouse e do sistema de telemetria/banco de dados. Interesse direto na aprovação e no bom desempenho do produto.

*Professores da disciplina PI1:* acompanham o desenvolvimento do projeto, avaliam as entregas ao longo do semestre e atuam como banca nos testes de integração e na apresentação final.

## Recursos do Projeto

### Membros da Equipe

| **Nome** | **Matrícula** | **Curso** | **E-mail** | **Funções** |
|----------|---------------|-----------|------------|-------------|
| Luiza da S. Pugas | 222025843 | Engenharia de Software | silvaluiza308@gmail.com | Software, Firmware |
| Geovana de Souza Braga | 202023707 | Engenharia Automotiva | gsb.souza.dara2001@gmail.com | Estrutura |
| Júlio César da Costa Santos | 222008922 | Engenharia Automotiva | juliocesar7152@gmail.com | Estrutura |
| João Marcos M. de Andrade | 222006113 | Engenharia de Software | andramas.10@gmail.com | Energia, Software |
| Karoline Luz do Conceição | 222006267 | Engenharia de Software | karolluz491@gmail.com | Software, Front |
| Ana Victória Guedes | 222002046 | Engenharia de Software | anaguedescosta009@gmail.com | Eletrônica |
| Anderson Luiz da Silva Maciel | 222021055 | Engenharia Eletrônica | andersonmaciel722@gmail.com | Eletrônica |
| Fábio Santos Araújo | 222022082 | Engenharia de Software | fabioofonteles1@gmail.com | Hardware |
| Marcus Vinicius Pereira dos Santos | 232038335 | Engenharia de Software | marcusviniciusxmvr@gmail.com | Eletrônica |
| Juan Costa Sandinano | 242075648 | | juan75indiano@gmail.com | |
| Jefferson de Souza Reis | 23201469 | | oliveira.jefferson@aluno.unb.br | |
| Murilo Vieira Ataíde Pimentel | 242024487 | | murilofpimentel@gmail.com | |
| Amanda de Moura | 221022220 | Engenharia de Software | amandademoura10@gmail.com | Software, Back/Front |
| Giovana Martins de Brito | 231038081 | | | |
| Wanjo Christopher Pacheco Escobar | 222037620 | Engenharia de Software | wanjo.christopher@gmail.com | Software, Back/Firmware |
| João Vitor Santos de Oliveira | 221022337 | Engenharia de Software | joaovitorso071@gmail.com | Eletrônica |

**Orientador:** Profa. Juliana Petrocchi Rodrigues

### Orçamento estimado (R$)

Discutam dentro da equipe a verba possível disponível para o desenvolvimento do projeto, com base na complexidade do projeto, na quantidade de membros e na realidade de cada um.

### Esforço estimado (horas)

**Período:** 02/09/2026 a 12/12/2026 (14 semanas e 3 dias).

Em sala, são 20 encontros presenciais (2 por semana, descontando os 4 feriados do período) de 1h50m cada, totalizando 36h40m (≈36,67h) de trabalho em conjunto.

Fora de sala, a dedicação individual foi estimada sobre duas bases de dias — apenas os 69 dias úteis do período (descontando feriados nacionais) ou esses mesmos dias somados aos 28 fins de semana (97 dias) — e duas cargas diárias, 30 minutos ou 1 hora. Somando o tempo em sala a cada combinação, chega-se a:

| Base de dias fora de sala | + 30 min/dia | + 1 hora/dia |
| -------------------------------------- | ------------- | ------------- |
| Dias úteis (69 dias) | 71h10m (≈71h) | 105h40m (≈106h) |
| Dias úteis + fins de semana (97 dias) | 85h10m (≈85h) | 133h40m (≈134h) |

Ou seja, o esforço total estimado por integrante ao longo do projeto (sala + fora de sala) varia entre aproximadamente **71h**, no cenário mais conservador, e **134h**, no cenário de maior dedicação diária considerando também os fins de semana.

Esses valores são uma estimativa inicial, baseada na carga horária prevista do semestre, e devem ser revisados ao longo do projeto conforme a necessidade real de cada fase.
