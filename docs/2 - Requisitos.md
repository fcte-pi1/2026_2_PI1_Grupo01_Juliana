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

### Requisitos funcionais

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| RF1 | Detecção de paredes | O sistema eletrônico deve detectar a presença de paredes ao redor do Micromouse por meio dos sensores, disponibilizando essas informações ao sistema de controle para auxiliar no mapeamento e na navegação pelo labirinto. | Must Have | Eletrônica | Deve funcionar considerando as características da pista (chão preto, paredes brancas com topo vermelho, 5 cm de altura, células de 18 cm). Os testes de detecção devem ocorrer antes da integração. |
| RF2 | Controle dos motores | O sistema eletrônico deve acionar e controlar os motores do Micromouse, permitindo sua movimentação, realização de curvas e paradas durante a navegação, garantindo compatibilidade elétrica entre microcontrolador, drivers e motores. | Must Have | Eletrônica | Deve ser validado por testes de movimento, curvas e paradas. |
| RF3 | Aquisição de dados de telemetria | O sistema eletrônico deve adquirir e disponibilizar ao sistema de controle os dados necessários à telemetria, como nível/consumo da bateria e dados de movimento (passos dos motores) utilizados para determinar a velocidade. | Must Have | Eletrônica | Os dados devem ser disponibilizados ao software/firmware para envio ao sistema web. Frequência de aquisição e formato devem ser definidos durante o projeto. |
| RF4 | Comunicação sem fio com a telemetria | O sistema eletrônico deve transmitir os dados coletados (bateria, velocidade, trajeto) via módulo Bluetooth para o sistema web de telemetria, garantindo o envio correto das informações a cada tentativa. | Must Have | Eletrônica | Módulo Bluetooth previsto no orçamento (item 10 do TAP). Complementa o RF3 (aquisição/disponibilização interna dos dados) e sustenta o RF24 do Firmware. |
| RF5 | Sinalização de estado do robô | O sistema eletrônico deve acionar indicadores visuais e sonoros (LED e buzzer) para sinalizar o estado do Micromouse (início do percurso, erro/colisão, conclusão do labirinto). | Should Have | Eletrônica | Relaciona-se ao RF25 do Firmware. |
| RF6 | Aquisição de dados de passo dos motores (odometria) | O sistema eletrônico deve fornecer ao microcontrolador a contagem de passos executados por cada motor, permitindo o cálculo da distância percorrida e da velocidade do Micromouse. | Must Have | Eletrônica | Sustenta o RF3 e o RF22 do Firmware. Como os motores são de passo (open-loop), a contagem pode ser feita pelo próprio firmware a partir dos pulsos enviados ao driver (RF11), sem sensor adicional (encoder). |

### Requisitos não funcionais

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| RNF1 | Confiabilidade elétrica | O sistema eletrônico deve operar durante o desafio sem reinicializações ou falhas elétricas não planejadas (causadas por ruído, quedas de tensão ou mau contato) que interrompam o funcionamento do Micromouse. | Must Have | Eletrônica | Deve ser verificado em testes contínuos e nos testes de integração. O sistema deve permanecer operacional pelos 10 minutos do desafio. |
| RNF2 | Estabilidade da alimentação | Os níveis de tensão entregues a cada subsistema eletrônico (lógica do microcontrolador, sensores e drivers dos motores) devem ser validados e mantidos dentro da faixa de operação de cada componente, inclusive durante os picos de corrente dos motores. | Must Have | Eletrônica | A regulação e a estabilidade da tensão propriamente ditas são responsabilidade da energia (itens 2 e 12). Este requisito cobre a validação, no lado da eletrônica, de que cada componente recebe tensão compatível com sua especificação. |
| RNF3 | Integração e compactação | Os componentes eletrônicos (PCB, sensores, conexões e drivers) devem ser organizados fisicamente de forma compacta e segura sobre a estrutura do robô, de modo que a disposição também permita a montagem e a manutenção dos componentes. | Must Have | Eletrônica | Trata da organização/arranjo dos componentes. |

## SOFTWARE

