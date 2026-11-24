# ADR-024 — O firmware usa a geometria medida do robô montado: giro de 90° em arco, meia-volta em manobra e centralização pelas paredes

**Data:** 24/11/2026 · **Assinou:** Jauzimm · **Substitui em parte:** [ADR-019](019-robo-anda-e-para-leitura-lateral-ao-entrar.md) (o momento da leitura lateral)

## Contexto

Na pista, o robô avançava "uma célula e um pouco", não girava no centro, batia nas paredes e, depois de bater, parava
de vez. O problema também apareceu no labirinto dos professores, com paredes de ~5 cm. As medidas do robô montado
(régua, 24/11/2026; [5.43](../evidencias/testes-integracao/firmware-hardware/5.43-medidas-do-robo-montado.txt))
mostraram que o firmware e o mundo simulado seguiam o CAD do [4.1](../4.1%20-%20Projeto%20conceitual%20de%20estruturas.md),
e não o robô:

| Grandeza | No código | Medido |
| --- | --- | --- |
| Pulsos do encoder por volta da roda | 640 (provisório) | ~574 (10 voltas marcadas: 5759 e 5743 à esquerda, 5716 e 5752 à direita) |
| Bitola (centro a centro dos pneus) | 92,5 mm | 87 mm |
| Eixo das rodas | no meio de um chassi 92 x 103 mm | 87,5 mm até a frente e 25 mm até a traseira (chassi 112,5 x 95 mm, 100 mm com os fios) |
| ToF frontal | 40 mm à frente do eixo | 87,5 mm (no nariz), a 4 cm do chão |
| ToF laterais | (20, ±30) mm, apontados a ±45° | (65, ±40) mm, apontados a ±90°, a ~4 cm do chão, em braços presos com fita |
| Erro dos ToF | nenhum | esquerdo lê ~2 cm a mais; os três leem de 30 a 40 mm abaixo de ~4 cm |
| Parede do labirinto | 12 mm | 15 mm (célula livre de 165 mm) |

Consequências diretas:

- Com 640 pulsos por volta, o avanço de uma célula andava ~20 cm e o giro de 90° saía com ~107°.
- O giro no lugar acontece em volta do eixo, que fica 31 mm atrás do centro do chassi: cada giro de 90° deslocava o robô
  ~31 mm para o lado e para a frente.
- A quina da frente gira num raio de ~100 mm, e do meio da célula à parede há 82,5 mm. O giro de 180° no lugar não cabe
  num corredor (o teste com o robô girado à mão num beco encostou), e o de 90° com parede à frente deixa ~7 mm de folga.
- Qualquer `MOV_FALHOU` (parada de emergência, roda travada, tempo esgotado) encerra a corrida como `stuck`.

A estrutura do robô não vai ser alterada; a correção é toda no firmware.

## Decisão

- **Constantes medidas** em `include/config/robo.h`: 574 pulsos por volta, bitola de 87 mm, a geometria do chassi
  (`EIXO_ATE_FRENTE_MM`, `EIXO_ATE_TRAS_MM`, `MEIA_LARGURA_*`, `EIXO_RECUO_MM`), a posição e o ângulo dos ToF, a
  parede de 15 mm, e cruzeiro de 250 mm/s com 1000 mm/s² (antes 400 mm/s e 2000 mm/s²).
- **Repouso:** parado, o robô fica com o chassi centrado na célula (como é posto na largada), e o eixo fica
  `EIXO_RECUO_MM` atrás do centro. A execução (`lib/execucao`) calcula cada ação a partir disso.
- **Giro de 90° em arco:** o robô recua até o eixo ficar 90 mm atrás do centro e faz um quarto de círculo de raio
  90 mm (`mov_arco`). A quina passa a ~10 mm da parede da frente e longe dos postes, e o robô termina no meio do
  corredor novo, sem o deslocamento lateral do giro no lugar.
- **Meia-volta em manobra:** gira 15°, dá ré até o eixo ficar ~22 mm fora do meio, completa os 180° (nessa posição o
  giro cabe no corredor, com ~4 mm de folga de cada lado), e um "S" (gira 15°, anda na diagonal, desgira) volta o robô
  para perto do meio.
- **Centralização no avanço:** um observador estima a posição lateral do eixo e o rumo do robô em relação ao corredor.
  A odometria prevê; cada leitura das paredes laterais corrige, comparada com a estimativa de ~50 ms antes (o atraso
  da mediana). Leituras impossíveis (acima de 85 mm, ou que discordam mais de 20 mm, como no poste de um canto) são
  descartadas, a não ser que três seguidas concordem. Leituras abaixo de 40 mm não medem, mas limitam: a parede está
  no máximo a 2 x leitura − 40 mm. O rumo pedido leva o eixo ao meio e cai a zero no fim do avanço; o rumo estimado é
  descontado no giro seguinte. A deriva de uma roda maior que a outra é aprendida entre os avanços.
- **Parede da frente:** o fim do avanço segue a distância medida (`MOV_GANHO_FRONTAL` 0,5), só com leituras a partir
  de 50 mm. Uma parada de emergência a até 35 mm do fim previsto conta como chegada, não como falha.
- **Leitura lateral:** com os laterais a 90°, o sensor mede a própria célula; o avanço guarda as paredes laterais
  quando os sensores passam pelo meio da célula nova (`EXEC_LEITURA_LATERAL_RESTANTE_MM`).
- **ToF:** o driver real desconta o erro medido de cada sensor (`TOF_*_DESVIO_MM`).
- **Mundo simulado:** o robô passa a ser o retângulo medido, com o eixo fora do centro; `sim_definir_tof_curto`
  imita a leitura comprimida perto da parede.

## Alternativas descartadas

- **Mover o eixo para o meio do chassi.** Resolveria a geometria, mas a equipe decidiu não mexer na estrutura.
- **Girar no lugar com o eixo no limite da parede da frente.** Deixava ~7 mm até a parede e até o poste do lado aberto,
  e o robô ~22 mm fora do meio: no mundo simulado, um erro de rumo de 5° já encostava.
- **Corrigir só a posição lateral, sem o rumo.** Sem estimar o rumo, com 1% de diferença entre as rodas e sem ruído,
  o robô não completou nenhuma de 36 corridas no mundo simulado sem encostar.

## Consequências

- Resultado no mundo simulado, nos 12 labirintos do FIRM-02 (`-dir`), 3 sementes de ruído cada, com a leitura
  comprimida perto da parede (corridas sem encostar, de 36):

  | Ruído do ToF | Rodas iguais | Direita +0,5% | Direita +1% | Direita +2% |
  | --- | --- | --- | --- | --- |
  | 0 | 36 | 36 | 36 | 33 |
  | ±3 mm | 36 | 36 | 35 | 31 |
  | ±5 mm | 32 | 31 | 29 | 26 |

  O teste `test_corridas_andando_nos_labirintos_do_projeto` fixa ±3 mm e +0,5%. Com mais ruído ou mais diferença entre
  as rodas, as folgas de ~4 mm da meia-volta e de ~10 mm do arco ficam no limite.
- Uma curva leva mais tempo que antes (recuo de ~59 mm, arco e avanço), e a meia-volta, várias etapas.
- Os valores do arco, da meia-volta e dos ganhos foram ajustados no mundo simulado. Na pista, falta conferir o ângulo
  real dos giros no chão (o teste com transferidor foi adiado) e a leitura dos laterais tortos.
- Os sensores ficam a ~4 cm do chão, acima das paredes de 3,5 cm do labirinto da equipe: lá a detecção de parede não é
  confiável. Os ensaios devem usar paredes de pelo menos ~5,5 cm.
- Exige regravar o robô (`esp32_real` ou `esp32_real_bt`).
