# Movimentação em pista: o que foi feito, o que se obteve e o que falta (26 e 27/11/2026)

Resumo do trabalho de regulagem do Ratatouille em pista, na branch `fix/movimentacao-lenta-bt`, para quem for
continuar com o robô. Tudo aqui saiu de corridas reais por Bluetooth, com o registro da ponte em
`src/backend/logs/ponte/`. A regra adotada foi: **a pista decide**; o simulador serve só para conferir que nada
quebrou.

## 1. Como gravar e rodar

Dentro de `src/firmware` (no Windows com acento no nome do usuário, use `tools\pio.cmd` no lugar de `pio`):

| Para quê | Comando |
|---|---|
| Corrida, com a regulagem de pista | `tools\pio.cmd run -e esp32_pista_bt -t upload --upload-port COM6` |
| Bancada (calibrar giro e reta) | `tools\pio.cmd run -e esp32_calibracao_pista_bt -t upload --upload-port COM6` |
| Testes no PC (Docker) | `docker run --rm -v "<pasta src/firmware>:/fw" pi1-pio-native pio test -e native` |

- Grave com o cabo USB e a **bateria desligada**. A corrida é com a bateria ligada e sem cabo.
- A regulagem de pista fica na seção `[pista]` do `platformio.ini`. Os padrões de `include/config/robo.h` continuam
  os do simulador, de propósito.
- **Espessura da parede**: a última linha do ambiente `esp32_pista_bt` traz `PAREDE_ESPESSURA_MM=18` (labirinto de
  casa). Para o labirinto oficial (parede de 1,2 cm), troque por 12 antes de gravar. Compila, mas **ainda não rodou
  em pista com 12**.
- A ponte e a bancada usam a mesma porta Bluetooth: pare a ponte antes de usar a bancada.
- Toque curto no BOOT apaga o mapa. Faça isso sempre que o labirinto mudar.

### Calibração do giro (refazer no piso da prova)

O piso muda quanto a roda patina no giro, e o encoder não vê isso. Leva uns 3 minutos:

1. Grave a bancada e ponha o robô no piso, alinhado com uma referência, com ~25 cm livres em volta.
2. Mande a tecla `Q` (10 giros de 90°). Ele deve terminar virado exatamente para trás (2,5 voltas).
3. Meça quanto faltou ou sobrou. Novo `BITOLA_MM` = atual × 900 ÷ (graus que ele girou de verdade).
4. Troque `BITOLA_MM` na seção `[pista]` e grave a corrida.

No piso do labirinto 2 (27/11): com 85,5 mm ele girou 830° (faltaram 7° por giro); com 92,7 mm, 925°. O valor
adotado, 90,5 mm, vem da conta entre as duas medidas e não foi conferido com um terceiro teste.

## 2. O que mudou no firmware

| Mudança | Por quê (o que a pista mostrou) |
|---|---|
| Ré de uma casa para sair de beco reto, em vez de meia-volta | O giro de 180° no corredor tem ~3 mm de folga por lado e raspava |
| Giro de 90° no lugar, perto do centro, quando a frente está aberta ou depois de uma ré | 11 de 47 arcos travaram em 26/11; no lugar não depende de onde o arco começa |
| Antes do arco, ré até o sensor da frente ler uma distância fixa | Arcos que começavam perto demais da parede da frente enroscavam |
| Veto pela leitura: a navegação não manda o robô contra uma parede que o sensor está vendo | Ele empurrava parede por 3 s seguindo um mapa errado |
| Nova tentativa, uma vez por ação, desfazendo o movimento que enroscou | Um enrosco simples derrubava a corrida inteira |
| Na nova tentativa de um avanço, sai virado um pouco para o lado livre | Só desvirar levava à mesma pose e ao mesmo enrosco (27/11) |
| Na ré, mede o ângulo pela parede do lado e o giro seguinte desconta | Ele voltava do beco 4° a 14° torto com os encoders dizendo 2° |
| Antes do arco, se estiver 5° ou mais torto, endireita no lugar | Um arco que começou ~18° torto enroscou na parede de fora |
| Distância entre rodas, velocidade do giro, espessura da parede e outros viraram opções de compilação | Regular em pista sem mexer nos padrões do simulador |
| Correção do sinal do rumo depois de um arco de ré | A nova tentativa do arco saía para o lado contrário |
| Aprendizado do "puxão para um lado" mais lento (0,7 → 0,15, só na regulagem de pista) | A compensação pulava de um extremo ao outro a cada casa |

Testes automáticos novos: arco de ré desfaz o arco; ré mede o rumo pelas paredes. Situação da suíte do PC em
27/11: 422 de 424 passam; o que falha, `test_sequencia_de_primitivas_chega_na_celula_certa`, já falhava antes
dessas mudanças.

## 3. O que se obteve

- **Labirinto 1 (parede de 1,5 cm, piso claro)**: é onde ele vai melhor. Na última sessão de 27/11, depois do
  último ajuste, foram 13 corridas em vários desenhos: 8 chegaram ao objetivo e 5 falharam com "preso".
- **Labirinto 2 (parede de 1,8 cm, piso escuro)**: chega em alguns desenhos e falha em outros. O piso é o
  problema principal: numa reta os encoders contaram a casa inteira e o robô estava 76 mm antes (roda patinando).
- A calibração do giro foi o que mais mudou o resultado no labirinto 2: antes dela o robô não passava do primeiro
  beco.
- O labirinto oficial (parede de 1,2 cm) nunca foi testado.

## 4. O que falta (em ordem do que mais pesa)

1. **Curva em arco para a direita termina rente à parede direita.** Nos registros de 27/11 os arcos para a
   direita terminam com ~20 mm da parede direita e 60 a 80 mm da esquerda; os da esquerda terminam centrados.
   A roda esquerda (a de fora nessa curva) costuma ficar alguns pulsos abaixo do pedido. Mexer no ponto de início
   do arco (157 → 132 mm) não resolveu. Hipótese ainda não testada: a roda esquerda é mais fraca, o que também
   explicaria o robô puxar de leve para a direita na reta.
2. **Aderência no piso.** Limpar rodas e piso antes de correr. Não há tratamento no firmware para roda patinando
   em reta: ele só percebe quando o tempo do movimento (3 s) estoura.
3. **Detectar o enrosco mais cedo.** Hoje ele força até 3 s antes de desistir do movimento. Ver ideias abaixo.
4. **Testar com parede de 1,2 cm** e repetir a calibração de giro no piso da prova.
5. **Testes automáticos** para a ré, o veto e o giro no centro: essas partes só foram verificadas em pista.
6. O sensor direito lê valores estranhos em alguns postes (95 a 140 mm); conferir a altura das paredes de casa
   contra os 5 cm do oficial.

## 5. Ideias de plano B para casos específicos (ainda não implementadas)

Em ordem de quanto valem pelo esforço:

- **Perceber que está preso sem esperar 3 s.** Com parede à frente, comparar o que os encoders dizem que andou com
  o quanto a leitura da frente diminuiu: se as rodas "andaram" 30 mm e a parede não chegou mais perto, ele está
  patinando ou enroscado, e pode desistir do movimento na hora.
- **Depois de recuar, olhar antes de tentar de novo.** Parar um instante, ler os três sensores e escolher: se um
  lado está a menos de ~25 mm, virar alguns graus para longe dele; se a frente está perto demais para a curva,
  recuar mais. Hoje a nova tentativa usa só os encoders.
- **Segunda nova tentativa diferente da primeira.** Hoje é uma por ação. A segunda poderia trocar de manobra: arco
  que falhou duas vezes vira giro no lugar; giro no lugar que falhou vira ré e arco.
- **Não se perder depois de um enrosco.** Quando um movimento termina por tempo, conferir as paredes lidas contra
  o mapa antes de aceitar que mudou de casa. Hoje ele assume que chegou, e é assim que se perde.
- **Aproveitar o mapa entre as 3 tentativas.** Pelo regulamento são até 3 tentativas por labirinto em 10 minutos,
  e o robô volta à largada a cada falha. O firmware hoje não tem esse caso:
    - depois de um **sucesso**, a largada seguinte apaga o mapa e explora tudo de novo (`corrida.c`, `nav_iniciar`
      na nova execução);
    - depois de uma **falha**, a largada seguinte é uma retomada: ele mantém o mapa, mas parte da casa onde
      parou. Se o robô foi levado de volta para A1, a posição que ele acha que tem está errada (ver seção 6);
    - o toque curto no BOOT volta a posição para A1, mas apaga o mapa junto.

  O que falta é a quarta opção: **voltar para A1 mantendo as paredes já vistas**, e na tentativa seguinte ir direto
  pelo caminho conhecido. É a melhoria de maior retorno para a prova. Cuidado: um mapa com parede errada leva a
  uma falha repetida, então a tentativa seguinte deve continuar conferindo o mapa com os sensores (o veto já faz
  isso), e vale descartar as paredes da casa onde ele enroscou.
- **Marcar a casa onde enroscou como cara.** Se houver outro caminho, a navegação o prefere na tentativa seguinte.
- **Modo cauteloso.** Depois da primeira falha, a tentativa seguinte anda mais devagar (as velocidades já são
  opção de compilação; falta poder trocar durante a prova, que o regulamento só permite sem mudar o código).

## 6. Interface e backend (investigado em 27/11 com os registros da ponte e os dados gravados)

Corrigido nesta branch:

| Sintoma | Causa | Correção |
|---|---|---|
| `largar` enviado duas vezes a cada largada | As linhas de depuração do robô (`# dec`, `# mov`) recebem resposta 422 da API, e essa resposta levava de novo os comandos pendentes | A 422 leva só a interrupção; `largar` e `health_check` esperam a próxima resposta 200 |
| Rajada de `largar` (24 seguidos, 1 por segundo) e tentativa "em andamento" com o robô parado | O robô tinha reiniciado e perdido a aprovação do health-check; o backend aceitava a largada com o health-check antigo | Se o robô reiniciou depois do último health-check, a largada é recusada pedindo nova verificação |
| Paredes sumindo no desenho do labirinto (execução encerrada e detalhe) | A tela usava só as paredes da última passagem por cada casa, e cada passagem traz 3 lados | A tela soma as paredes de todas as passagens e abre o lado por onde o robô passou |

Verificação: backend com 515 testes passando (eram 511); frontend com checagem de tipos, lint e build sem erro e
268 testes passando quando rodados em sequência. Na execução em paralelo houve 4 estouros de tempo de 5 s em testes
que passam sozinhos; não foi medido se isso já acontecia antes. As correções do backend só valem depois de
reconstruir o contêiner da API, e **nenhuma foi conferida com o robô**.

Não corrigido, e é a causa mais provável de "aparece errado" e "acaba antes":

- **Largada depois de uma falha.** O firmware retoma da casa onde parou, e a tela abre uma execução nova desde A1.
  Exemplo gravado (execução 56, 27/11 17:55): depois da largada vieram só 4 casas, (2,1) a (3,3), e "sucesso" em
  6,6 s. Se o robô foi recolocado em A1, o desenho sai deslocado e ele declara sucesso onde acha que é o objetivo.
  Consertar exige mudar o firmware e o contrato (o `largar` dizer se é execução nova ou retomada).
  **Contorno até lá**: depois de uma falha, usar "Retomar" com o robô onde parou, ou dar o toque curto no BOOT
  (1 bipe) e só então "Nova execução" a partir de A1.

Outros pontos vistos, sem mudança:

- Durante cada movimento (~2 s) o marcador na tela fica uma casa atrás do robô: a posição só muda quando a casa
  termina.
- A tela encerra a tentativa por perda de sinal depois de 10 s sem dados, mesmo que o robô continue andando (queda
  real do Bluetooth na execução 62).
- A casa A1 normalmente não manda passo, então as paredes dela não são desenhadas.
