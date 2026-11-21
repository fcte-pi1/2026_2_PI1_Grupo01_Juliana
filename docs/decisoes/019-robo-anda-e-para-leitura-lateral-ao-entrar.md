# ADR-019 — O robô anda e para, a parede lateral é lida ao entrar na célula e o laço de movimento mantém o controle de posição

**Status:** Aceito  
**Data:** 2026-11-21

## Contexto

Até a FIRM-09 nenhuma tarefa acionava as rodas: a navegação decidia a ação e, na simulação, o robô era teletransportado para a célula seguinte (`sim_mundo_na_celula`). A FIRM-10 ([#141](https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/issues/141)) leva o firmware completo ao robô montado, então a navegação precisa mandar as rodas andarem.

Dois fatos pesaram:
- O ToF lateral a 45° mede a parede ~74 mm à frente do robô. No centro da célula, com o lado aberto, ele enxerga o poste do canto e devolve **parede falsa**; a leitura é confiável só logo ao entrar na célula (README do firmware, detecção de paredes). A corrida antiga só chegava ao objetivo porque o teletransporte já colocava o robô nessa faixa.
- O PID de velocidade por roda (`vel_passo`, FIRM-04) e o controle de posição do movimento (`mov_atualizar`, FIRM-05) já existem separados, cada um com testes.

## Decisão

- **Anda e para.** Parado no centro de cada célula, o robô espera 200 ms para a mediana dos ToF estabilizar, lê a frente, decide e executa a ação (`lib/execucao`: giro e depois avanço de uma célula). A velocidade máxima de corrida fica para depois.
- **A parede lateral da célula nova é guardada durante o avanço**, quando o robô já andou `EXEC_LEITURA_LATERAL_MM` (100 mm, cerca de 10 mm depois da fronteira); a parede da frente é lida parado no centro, onde o frontal é confiável. A primeira decisão de uma corrida ou retomada usa a leitura ao vivo (não houve entrada na célula).
- **O laço da navegação roda a 5 ms** (`MOV_PERIODO_MS`), o ciclo do controle de movimento. A simulação anda pela física do mundo simulado, sem teletransporte, e o teste de corrida do PC anda de verdade.
- **Falha de movimento** (roda travada ou fora do tempo) vira a falha automática `stuck`: a corrida vai a `failed`, grava a NVS e a retomada continua da célula.
- **O PID de velocidade não entra no laço do movimento.** O movimento segue com controle de posição e alimentação direta; o PID por roda é validado na bancada de calibração (tecla `v`, erro de até 5%).
- **Gravações da NVS limitadas** a `MEM_GRAVACOES_MAX` (600) por inicialização para a gravação por célula, para um robô que oscile entre células não gastar a flash (a gravação do fim da corrida e da falha continua).
- **Sentido das rodas** (fato de hardware, teste 5.21 do 7.5): só o motor e o encoder **esquerdos** precisam de `invertido`; o direito já estava certo.

## Alternativas descartadas

- **Ler a lateral no centro, parado.** Dá parede falsa quando o lado está aberto.
- **Trocar o laço do movimento pelo PID de velocidade.** Risco alto de desestabilizar um movimento já testado, sem ganho para os critérios de aceite; o PID continua verificável na bancada.
- **Corrida contínua, sem parar em cada célula.** Exige ler a lateral em movimento e planejar o próximo passo antes de parar; fica para a otimização de velocidade, depois da calibração.

## Consequências

- O teste `test_execucao` roda a corrida completa andando no mundo simulado e acusa regressões da navegação com movimento. O `esp32_sim` agora leva cerca de 1 s por célula.
- O robô retomado depois de `failed` assume que o operador o recolocou no centro da célula e virado para a direção gravada.
- Os valores de `config/robo.h` (pulsos, `EXEC_LEITURA_LATERAL_MM`, limiares de parede, stuck) serão fixados com a bancada `esp32_calibracao` na pista, e o resultado registrado no roteiro de testes de integração (7.5).
- A centralização pelos ToF laterais (`MOV_KP_CENTRO`) e a correção pelo frontal seguem desligadas até os ToF laterais funcionarem (defeito no conector da placa, teste 5.2).
