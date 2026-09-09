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

#### Limites e Restrições

- O Micromouse deve ser capaz de operar nos labirintos padronizados da disciplina, cujas células possuem 18 cm de lado e paredes com 5 cm de altura e 1,2 cm de espessura (chão preto com paredes brancas de topo vermelho).
- O sistema deve ser desenvolvido integralmente pela equipe, sendo proibido o uso de soluções prontas comerciais (kits de mercado).
- São proibidos sistemas de locomoção por propulsão a combustão ou foguete, e comportamentos como voar, pular ou escalar.
- Durante a operação no labirinto, o código-fonte e a memória não poderão ser alterados em tempo de execução.
- São proibidas intervenções humanas durante a corrida, exceto em casos de colisão ou mau funcionamento, permitindo-se apenas pequenos reparos (como limpeza de rodas) com o robô em repouso.
- O robô terá um limite de tempo estabelecido de 10 minutos para tentar resolver cada desafio durante os testes de integração e apresentação final.

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

 <img src="https://raw.githubusercontent.com/fcte-pi1/template/refs/heads/main/docs/figs/legenda-micro-mouse.png" alt="" width="450"> 

| Componente | Fornecedor | Modelo | Preço | Frete | Subtotal |
| ----- | ----- | ----- | :---: | :---: | :---: |
| 1.Sensor IF esquerdo | Eletrogate | HW-201 | R$5,90 | R$21,65 | R$27,55 |
| 2.Sensor infravermelho dianteiro esquerdo | Eletrogate | HW-201 | R$5,90 | R$21,65 | R$27,55 |
| 3.Suporte de pilhas AA(4- AA) | Eletrogate |  | R$5,90 | R$21,65 | R$27,55 |
| 4\. 2 Pilhas AA ON/OFF | Usinainfo |  | R$23,70 | R$22,55 | R$46,25 |
| 5.Motor de passo esquerdo | Mercado livre | Nema 11 | R$182 | R$0 | R$182 |
| 6.Interruptores configuráveis para software de controle | Mercado livre | Dip Switch 4 vias | R$20,82 | R$0 | R$20,82 |
| 7.Conector para o motor direito | Mercado livre | JST 4 vias | R$20 | R$0 | R$20 |
| 8.Sensor IF direito | Mercado livre | Lm 393 | R$19 | R$0 | R$19 |
| 9.Sendor IF dianteiro direito | Mercado livre | HW\_201 | R$8,50 | R$18,00 | R$26,5 |
| 10.Conector Bluetooth (módulo não incluído) | Mercado livre | HM\_10 BLE 4.0 | R$48,00 | R$0 | R$48 |
| 11.Pino LED 13/Buzzer (Buzzer não incluído) | Mercado livre | Ativo | R$6,50 | R$15,00 | R$21,5 |

| 12.Chave LED ON/OFF | Mercado livre | Gangorra c/ LED | R$12,00 | R$15,00 | R$27 |
| :---- | ----- | ----- | :---: | :---: | :---: |
| 13.Jumper para seleção de pilhas AA/bateria LiPo |  | Jumper padrão | R$2,00 | R$0 | R$2 |
| 14.Motor de passo direito | Mercado livre | Nema 11 | R$182 | R$0 | R$182 |
| 15.Chave Bateria LiPo ON/OFF |  | Slide Switch SPDT/SP3T | R$11,94 | R$0 | R$11,94 |
| 16.Conector para o motor esquerdo | Mercado livre | JST 4 vias | R$20 | R$0 | R$20 |
| 17.Placa PCB |  | FR4 Dupla face | R$ 100 | R$ 50 | R$ 150 |
| 18.Rodinhas |  | 2 (40 mm) e 1 giratória | R$20 | R$10 | R$ 30 |

**Total: R$ 889,66**



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
