# Requisitos

## ESTRURAS



## ENERGIA

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| 1 | Alimentação autônoma embarcada  | O sistema de energia deve alimentar todos os subsistemas embarcados (microcontrolador, sensores, motores e módulo de comunicação) a partir de uma fonte transportada pelo próprio Micromouse, sem cabos ou fontes externas.  | Alta (Must Have) | Energia  | Operação 100% autônoma exigida no slide 7\.  |
| 2 | Regulação das tensões de operação  | O sistema de energia deve fornecer, a partir da fonte, os níveis de tensão exigidos por cada subsistema (lógica do microcontrolador, sensores e drivers dos motores).  | Alta (Must Have) | Energia  |  |
| 3 | Seleção da fonte de energia  | O sistema de energia deve permitir alternar, por jumper, entre o suporte de pilhas AA e a bateria LiPo, sem alteração de solda ou troca de componentes.  | Média (Should Have) | Energia  | Já previsto no orçamento (item 13 do TAP). Garantir que as duas fontes nunca fiquem ligadas ao mesmo tempo.  |
| 4 | Separação entre potência e lógica  | O sistema de energia deve isolar a linha de alimentação dos motores da linha de alimentação da lógica, de modo que picos de corrente dos motores não provoquem reinicialização do microcontrolador nem leituras erradas dos sensores.  | Alta (Must Have) | Energia |  |
| 5 | Acionamento geral de energia  | O sistema de energia deve possuir uma chave liga/desliga geral, acessível com o robô montado, que corte a alimentação de todos os subsistemas.  | Alta (Must Have) | Energia  | Itens 12 e 15 do orçamento.  |
| 6 | Medição do nível de carga  | O sistema de energia deve disponibilizar ao microcontrolador um sinal proporcional à carga restante da fonte, permitindo o cálculo do nível e do consumo de bateria.  | Alta (Must Have) | Energia  | Alimenta o dado de telemetria "consumo de bateria" (slide 11).  |
| 7 | Sinalização do estado de energia  | O sistema de energia deve indicar visualmente que o robô está energizado e sinalizar quando a carga atingir o nível mínimo de operação.  | Baixa (Could Have)  | Energia  | Evita iniciar uma tentativa com carga insuficiente; usa o LED já previsto no orçamento.  |
| 8 | Proteção contra sobrecorrente e curto-circuito  | O sistema de energia deve interromper a alimentação em caso de curto-circuito ou corrente acima do limite de projeto, protegendo bateria e componentes.  | Alta (Must Have) | Energia  | Fusível, PTC ou limitação por driver, a definir no projeto conceitual. |
| 9 | Proteção contra inversão de polaridade  | O sistema de energia deve impedir danos aos componentes caso a fonte seja conectada com polaridade invertida.  | Média (Should Have) | Energia  | Risco real durante a troca de pilhas entre tentativas.  |
| 10 | Troca e recarga sem desmontagem  | O sistema de energia deve permitir a troca das pilhas ou a recarga da bateria sem desmontar a estrutura do Micromouse nem desconectar a PCB.  | Média (Should Have) | Energia  | Seria um pequeno reparo. |
| 11 | Autonomia de operação  | A fonte de energia deve manter o Micromouse em operação contínua por, no mínimo, os 10 minutos de uma tentativa, com margem para três tentativas consecutivas sem troca ou recarga.  | Alta (Must Have) | Energia  | Slides 21 e 22\.  |
| 12 | Estabilidade da tensão sob carga  | A tensão de alimentação da lógica não deve variar além de ±5% do valor nominal durante os picos de corrente dos motores, e o microcontrolador não deve reiniciar em nenhuma manobra.  | Alta (Must Have) | Energia  |   |
| 13 | Proteção da Bateria LiPo (BMS/Undervoltage)  | O sistema elétrico deve contar com circuito de proteção específico para impedir descarga profunda (abaixo da tensão mínima por célula) e sobrecarga da bateria LiPo, prevenindo danos permanentes e risco de incêndio.  | Alta (Must Have)  | Energia  |  |
| 14 | Filtragem e Desacoplamento de Ruído  | O circuito deve prever capacitores de desacoplamento e/ou filtros na linha de alimentação dos motores para mitigar ruídos elétricos (EMI) gerados pelo acionamento mecânico.  | Alta (Must Have)  | Energia  |  |
| 15 | Dimensionamento da Capacidade (Cálculo de mAh)  | A capacidade nominal da bateria escolhida deve ser justificada por cálculo prévio de consumo máximo e médio de todos os subsistemas, assegurando a autonomia de 10 minutos (com margem de segurança para tentativas extras).  | Média (Should Have)  | Energia  |  |
| 16 | Gerenciamento Térmico de Reguladores  | Os componentes de regulação de tensão (step-down/lineares) e os drivers de potência devem possuir margem térmica segura, utilizando dissipadores ou áreas de dissipação na PCB se o cálculo térmico exigir.  | Média (Should Have)  | Energia  |  |

## ELETRÔNICA



## SOFTWARE

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| RF01 |  |  |  |  |  |
| RF02 |  |  |  |  |  |
| RF03 | Visualizar histórico de execuções | O sistema deve apresentar para o usuário uma seção na aplicação que forneça o histórico de todas as execuções já realizadas. | Alta (Must Have) | Frontend | Em ordem decrescente de execução (da última para a primeira). |
| RF04 | Visualizar execução em tempo real | Caso haja uma execução com o status "health-check" ou "running", o sistema deve fornecer ao usuário os dados dessa execução na tela inicial, com o tempo da última atualização. | Média (Should Have) | Frontend | Ex.: mensagem "última atualização há 4 segundos". Sistema de polling. |
| RF05 | Visualizar os dados captados no health-check | O sistema deve fornecer ao usuário, durante a execução e após o término da mesma, os dados coletados quando o status for "health-check" em uma área específica, separada do resto dos dados coletados na página da execução específica. | Alta (Must Have) | Frontend | Conectado ao RNF03. |
| RF06 | Fornecer dados de uma execução em tempo real | O sistema deve ser capaz de consumir os dados de telemetria de uma execução em status "health-check" ou "running". | Média (Should Have) | Backend |  |


