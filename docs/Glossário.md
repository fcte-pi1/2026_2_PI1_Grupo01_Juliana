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
| **Área de Objetivo** | Célula de chegada do labirinto, sempre no canto diametralmente oposto ao de partida. Como o tamanho do labirinto não é conhecido de antemão, sua posição depende do tipo de labirinto descoberto durante a execução. Quando o Micromouse a alcança e confirma a borda, a execução termina com sucesso. |
| **Canal do Micromouse** | Componente do backend que recebe as mensagens do Micromouse, independentemente do meio de transmissão, e devolve na resposta os comandos de iniciar e parar. |
| **Célula** | Unidade quadrada do labirinto, de 18 x 18 cm, identificada por coluna e linha (ex.: A1, B2) e usada para registrar a posição do Micromouse. |
| **Encerrar** | Finalizar manualmente uma execução em andamento. O operador envia o comando de parar pela interface web, ou toca no botão BOOT do robô, e a execução é registrada como falha com o motivo encerrado_operador. |
| **Execução** | Uma passagem do Micromouse por um labirinto: começa quando o sistema recebe a primeira mensagem do robô, em health-check, e termina em sucesso ou falha. Evitar: corrida, run. |
| **Failed** | Estado final da execução em que o Micromouse não alcançou a área de objetivo, seja por estouro do tempo limite, seja por encerramento feito pelo operador. |
| **Health-check** | Estado inicial da execução, em que o Micromouse verifica seus componentes (bateria, sensores e rodas) antes de iniciar o trajeto. |
| **Labirinto** | Pista formada por células de 18 x 18 cm, com piso preto e paredes brancas de topo vermelho, nos formatos 4x4, 8x4 ou 12x4. |
| **Micromouse** | Robô autônomo que explora o labirinto com o algoritmo flood fill (método Adachi) até a área de objetivo, mapeando as paredes e enviando telemetria ao sistema web. |
| **Motivo de Falha** | Causa registrada quando uma execução termina em falha: colisão (collision), tempo excedido (time_exceeded), travamento (stuck), saída da pista (out_of_track), falha de componente (falha_componente, com o componente que falhou) ou encerramento pelo operador (encerrado_operador). |
| **Operador** | Membro da equipe que acompanha a execução pela interface web e pode encerrá-la manualmente quando necessário. |
| **Prova de Borda** | Confirmação de que uma linha inteira de paredes é o limite externo do labirinto: se todas as paredes dessa linha estão fechadas, nada além dela é alcançável e, como a área de objetivo sempre é alcançável, ali termina o labirinto. |
| **Rejeitar** | Recusar o início de uma execução que não corresponde à execução em andamento, registrando-o como tentativa rejeitada. |
| **Retomada** | Evento registrado quando o Micromouse, após uma falha de componente, refaz o health-check depois de 5 segundos e obtém resultado positivo. O robô continua de onde parou e a execução segue em vez de ser perdida. Evitar: reinício de execução, nova execução. |
| **Retomar** | Continuar a execução em andamento, a partir da célula onde o robô parou, após uma falha de componente seguida de um health-check positivo. |
| **Running** | Estado da execução em que o Micromouse já iniciou o trajeto pelo labirinto. |
| **Sistema de Telemetria** | Sistema web desenvolvido pela equipe que recebe, valida, armazena e exibe os dados enviados pelo Micromouse. Envia ao robô apenas os comandos de iniciar e parar, nunca comandos de navegação. |
| **Success** | Estado final da execução em que o Micromouse alcançou a área de objetivo. |
| **Telemetria** | Conjunto de dados enviados pelo Micromouse durante a execução, como célula atual, nível de bateria, velocidade, status e o instante de envio. |
| **Tempo Limite** | Duração máxima de 10 minutos de uma execução. Ao ser ultrapassado, a execução é encerrada como falha com o motivo time_exceeded. |
| **Tentativa Rejeitada** | Início de execução recusado pelo sistema por não corresponder à execução em andamento (por exemplo, uma retomada sem execução em andamento, ou uma nova execução enquanto outra ainda está em andamento). Não é uma execução e não entra no histórico. Evitar: execução recusada, execução rejeitada. |
| **Tipo de Labirinto** | Formato do labirinto (4x4, 8x4 ou 12x4). O Micromouse o descobre durante a execução pela extensão das células visitadas e o envia como "indeterminado" até lá. |
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
