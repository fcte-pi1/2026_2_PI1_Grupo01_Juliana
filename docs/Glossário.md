# Glossário

## Introdução

Um glossário é uma ferramenta importante em diferentes áreas do conhecimento, pois contribui para tornar a compreensão e a comunicação mais claras e eficientes. Trata-se de uma lista de termos acompanhados de suas respectivas definições, organizada de forma alfabética ou por temas. Seu principal objetivo é explicar conceitos, expressões técnicas ou palavras específicas que podem não ser familiares ou que geram dúvidas para quem está começando ou se aprofundando em determinado assunto.

## Objetivos

O principal objetivo deste glossário é **padronizar e tornar explícito o vocabulário** utilizado no projeto do **Micromouse** do Grupo 01, reduzindo ambiguidades e garantindo consistência entre os requisitos, os diagramas e a documentação das subequipes.

Além disso, o glossário busca:

- Facilitar o entendimento dos termos do domínio (labirinto, execução, telemetria) para quem não participou das discussões da equipe.
- Alinhar o significado de conceitos que aparecem com nomes diferentes nos artefatos, como "corrida", "tentativa" e "execução".
- Apoiar a escrita de requisitos, do diagrama entidade-relacionamento e das decisões de arquitetura com definições únicas e consistentes.

## Termos

| Termo | Definição |
| :--- | :--- |
| **Área de Objetivo** | Célula de chegada do labirinto, sempre no canto diametralmente oposto ao de partida. Sua posição depende do tipo de labirinto selecionado no DIP switch. Quando o Micromouse a alcança, a execução termina com sucesso. |
| **Canal do Micromouse** | Componente do backend que recebe, pela ponte serial Bluetooth, as mensagens do Micromouse e envia ao robô o único comando previsto: a interrupção, quando o operador encerra a tentativa manualmente. |
| **Célula** | Unidade quadrada do labirinto, de 18 x 18 cm, identificada por coluna e linha (ex.: A1, B2) e usada para registrar a posição do Micromouse. |
| **Encerrar** | Finalizar manualmente a tentativa em andamento. Pela interface web, o operador usa **Encerrar tentativa**, escolhe o motivo (collision, stuck ou out_of_track) e o sistema envia a interrupção ao robô, registrando o motivo e a origem encerrado_operador; pelo botão BOOT do robô, a tentativa termina com o motivo encerrado_operador. Em ambos os casos, a execução lógica continua e pode ser retomada. |
| **Execução** | Uma passagem do Micromouse por um labirinto. No sistema, corresponde a uma **tentativa** dentro de uma **execução lógica**. Evitar: corrida, run. |
| **Execução Lógica** | Conjunto das até 3 tentativas do Micromouse em um mesmo tipo de labirinto, aberto pelo operador com **Nova execução**. Termina concluída quando uma tentativa alcança a área de objetivo, ou cancelada pelo tempo limite, pela perda de comunicação ou pelo esgotamento das tentativas. |
| **Failed** | Estado final da tentativa em que o Micromouse não alcançou a área de objetivo, seja por falha no trajeto, falha de componente, perda de comunicação, estouro do tempo limite ou encerramento feito pelo operador. |
| **Health-check** | Estado inicial da tentativa, em que o Micromouse verifica seus componentes (bateria, sensores e rodas) antes de iniciar o trajeto. |
| **Interrupção** | Único comando que o Sistema de Telemetria envia ao Micromouse, pela ponte Bluetooth, quando o operador encerra a tentativa manualmente. O robô para e aguarda. |
| **Labirinto** | Pista formada por células de 18 x 18 cm, com piso preto e paredes brancas de topo vermelho, nos formatos 4x4, 8x4 ou 12x4. |
| **Micromouse** | Robô autônomo que explora o labirinto com o algoritmo flood fill (método Adachi) até a área de objetivo, mapeando as paredes e enviando telemetria ao sistema web. |
| **Motivo de Falha** | Causa registrada quando uma tentativa termina em falha: colisão (collision), travamento (stuck), saída da pista (out_of_track), tempo excedido (time_exceeded), falha de componente (falha_componente, com o componente que falhou), bateria baixa (low_battery), health-check reprovado (health_check_failed), perda de comunicação (link_lost) ou encerramento pelo operador (encerrado_operador). No encerramento manual pela interface, o operador escolhe o motivo e a tentativa recebe também a origem encerrado_operador. |
| **Operador** | Membro da equipe que acompanha a execução pela interface web e pode encerrá-la manualmente quando necessário. |
| **Rejeitar** | Recusar uma ação que não corresponde ao estado da execução lógica, registrando-a como tentativa rejeitada. |
| **Retomada** | Evento registrado quando o operador aciona **Retomar tentativa (*n*/3)** após uma tentativa em falha: abre-se uma nova tentativa da mesma execução lógica e o robô continua da célula onde falhou, com o mapa preservado. Consome uma das 3 tentativas. Evitar: reinício de execução, nova execução. |
| **Retomar** | Abrir uma nova tentativa da execução lógica em andamento após uma falha, continuando da célula onde o robô parou e aproveitando o mapa já construído. |
| **Running** | Estado da tentativa em que o Micromouse já iniciou o trajeto pelo labirinto. |
| **Sistema de Telemetria** | Sistema web desenvolvido pela equipe que recebe, valida, armazena e exibe os dados enviados pelo Micromouse. Envia ao robô apenas o comando de interrupção no encerramento manual, nunca comandos de navegação. |
| **Success** | Estado final da tentativa em que o Micromouse alcançou a área de objetivo. |
| **Telemetria** | Conjunto de dados enviados pelo Micromouse durante a execução, como célula atual, nível de bateria, velocidade, status e o instante de envio. |
| **Tempo Limite** | Duração máxima de 10 minutos no total para cada tipo de labirinto, contada desde **Nova execução** e somando as até 3 tentativas da execução lógica e os intervalos entre elas. Ao ser ultrapassado, a tentativa aberta falha com o motivo time_exceeded e a execução lógica é cancelada. |
| **Tentativa** | Cada passagem do Micromouse dentro de uma execução lógica, numerada de 1 a 3. Começa no health-check e termina em success ou failed. Evitar: corrida, run. |
| **Tentativa Rejeitada** | Ação recusada pelo sistema por não corresponder ao estado da execução lógica (por exemplo, uma retomada sem tentativa em falha, uma quarta tentativa ou uma nova execução enquanto outra ainda está em andamento). Não é uma tentativa e não entra no histórico. Evitar: execução recusada, execução rejeitada. |
| **Tipo de Labirinto** | Formato do labirinto (4x4, 8x4 ou 12x4). Selecionado no DIP switch do robô antes de ligá-lo. O operador também o informa em **Nova execução**, apenas no backend, para que a web desenhe a grade do labirinto e confira o DIP switch; essa informação nunca é enviada ao robô pela web. |
| **Trajeto** | Sequência cronológica de células visitadas pelo Micromouse em uma execução, que permite reconstruir o percurso completo. |

<font size="2"><p style="text-align: center">Fonte: [Wanjo Christopher Paraizo Escobar](https://github.com/wChrstphr), 2026.</p></font>

---

## Referências Bibliográficas

> <a id="REF1">1.</a> Tema do projeto de PI1 2026.2: Micromouse. Disponível em: https://docs.google.com/presentation/d/1hRx0g9mpVGQbTv44gLEoAyf4b4kKM4vnrABTnG8HlUk/edit?usp=sharing. Acesso em: 23 de set. 2026.

> <a id="REF2">2.</a> Grupo 01. Requisitos do projeto. Disponível em: [Requisitos](2%20-%20Requisitos.md). Acesso em: 23 de set. 2026.

## Histórico de Versões

| Versão | Data | Descrição | Autor(es) | Revisor(es) | Detalhes da revisão |
| :----: | :--: | --------- | ----------- | ------ | :---: |
| 1.0 | 23/09/2026 | Criação do glossário | [Wanjo Christopher Paraizo Escobar](https://github.com/wChrstphr) | - | - |
| 1.1 | 25/09/2026 | Atualização com a descoberta do labirinto em execução, a retomada, os comandos de iniciar e parar e o flood fill | [Wanjo Christopher Paraizo Escobar](https://github.com/wChrstphr) | - | - |
| 1.2 | 27/09/2026 | Alinhamento ao modelo de execução lógica com até 3 tentativas: inclusão de Execução Lógica, Tentativa e Interrupção; revisão de Canal do Micromouse, Encerrar, Execução, Failed, Motivo de Falha, Rejeitar, Retomada, Retomar, Sistema de Telemetria, Tempo Limite, Tentativa Rejeitada e Tipo de Labirinto; estados passam a se referir à tentativa | [Wanjo Christopher Paraizo Escobar](https://github.com/wChrstphr) | - | - |
| 1.3 | 02/10/2026 | Tipo de labirinto selecionado no DIP switch, conforme esclarecimento do professor: revisão de Área de Objetivo e Tipo de Labirinto; remoção de Prova de Borda | [Wanjo Christopher Paraizo Escobar](https://github.com/wChrstphr) | - | - |
