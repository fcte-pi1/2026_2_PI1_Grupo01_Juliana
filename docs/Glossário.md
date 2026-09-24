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
| **Área de Objetivo** | Célula de chegada do labirinto, sempre no canto inferior oposto ao de início. Quando o Micromouse a alcança, a execução termina com sucesso. |
| **Célula** | Unidade quadrada do labirinto, de 18 x 18 cm, identificada por coluna e linha (ex.: A1, B2) e usada para registrar a posição do Micromouse. |
| **Encerrar** | Finalizar manualmente uma execução em andamento. É feito pelo operador, que registra a execução como falha e informa o motivo. |
| **Execução** | Uma passagem do Micromouse por um labirinto: começa quando o sistema recebe a primeira mensagem do robô, em health-check, e termina em sucesso ou falha. Evitar: corrida, run. |
| **Failed** | Estado final da execução em que o Micromouse não alcançou a área de objetivo, seja por estouro do tempo limite, seja por encerramento feito pelo operador. |
| **Health-check** | Estado inicial da execução, em que o Micromouse verifica seus componentes (bateria, sensores e rodas) antes de iniciar o trajeto. |
| **Labirinto** | Pista formada por células de 18 x 18 cm, com piso preto e paredes brancas de topo vermelho, nos formatos 4x4, 8x4 ou 12x4. |
| **Micromouse** | Robô autônomo que explora o labirinto com o algoritmo DFS até a área de objetivo, mapeando as paredes e enviando telemetria ao sistema web. |
| **Motivo de Falha** | Causa registrada quando uma execução termina em falha: colisão (collision), tempo excedido (time_exceeded), travamento (stuck) ou saída da pista (out_of_track). |
| **Operador** | Membro da equipe que acompanha a execução pela interface web e pode encerrá-la manualmente quando necessário. |
| **Rejeitar** | Recusar o início de uma execução que não corresponde à execução em andamento, registrando-o como tentativa rejeitada. |
| **Retomada** | Evento registrado quando o Micromouse, após uma falha de componente, volta a operar com um health-check positivo no mesmo labirinto. A execução continua em vez de ser perdida. Evitar: reinício de execução, nova execução. |
| **Retomar** | Continuar a execução em andamento após uma falha de componente seguida de um health-check positivo no mesmo labirinto. |
| **Running** | Estado da execução em que o Micromouse já iniciou o trajeto pelo labirinto. |
| **Sistema de Telemetria** | Sistema web desenvolvido pela equipe que recebe, valida, armazena e exibe os dados enviados pelo Micromouse, sem enviar comandos ao robô. |
| **Success** | Estado final da execução em que o Micromouse alcançou a área de objetivo. |
| **Telemetria** | Conjunto de dados enviados pelo Micromouse durante a execução, como célula atual, nível de bateria, velocidade, status e o instante de envio. |
| **Tempo Limite** | Duração máxima de 10 minutos de uma execução. Ao ser ultrapassado, a execução é encerrada como falha com o motivo time_exceeded. |
| **Tentativa Rejeitada** | Início de execução recusado pelo sistema por não corresponder à execução em andamento (por exemplo, quando as mensagens do Micromouse informam outro labirinto). Não é uma execução e não entra no histórico. Evitar: execução recusada, execução rejeitada. |
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
