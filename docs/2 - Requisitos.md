# Requisitos

## ESTRURAS

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| 1 | Envelope Dimensional Limite | A estrutura não pode exceder 16,5 cm de comprimento nem 16,5 cm de largura. Não há limite para a altura.  | Alta | Estrutura | Restrição física estrita da competição. Recomendado projetar com margem de segurança.  (ex:14 cm X 14 cm). |
| 2 | Compatibilidade com o Chão e Paredes | O chassi/rodas deve operar em chassi de piso preto de MDF e interagir com paredes de 5 cm de altura e 1,2 cm de espessura.  | Alta | Estrutura | Garantir vão livre em relação ao solo adequado e evitar colisão do chassi com as paredes.  |
| 3 | Atrito e Tração das Rodas   | Selecionar e integrar pneus/rodas com alto coeficiente de atrito com a superfície de piso pintada de preto.   | Alta | Estrutura |  Permite acelerações e frenagens sem derrapagem, garantindo a precisão da odometria.  |
| 4 | Suporte e Posicionamento dos Sensores  | A estrutura deve fornecer suportes rígidos e calibrados para a fixação dos sensores de distância sensores de linha/cor.   | Alta | Estrutura |  O alinhamento dos sensores deve ser fixo e resistente a vibrações para não comprometer o mapeamento.  |
| 5 | Proteção contra Danos ao Labirinto   |  O chassi deve possuir compartimentos adequados para fixação segura dos componentes. | Alta | Estrutura |  Manter o centro de gravidade baixo para estabilidade em curvas rápidas.   |
| 6 | Facilidade de Manutenção e Acesso   | Deve permitir rápido acesso/troca da bateria e facilidade de limpeza/manutenção das rodas durante as pausas.   | Alta | Estrutura | Pequenos reparos e limpeza das rodas são permitidos em repouso.    |
| 7 | Modelagem  e Manufatura   | Toda a estrutura do robô e componentes mecânicos deverão ser modelados em CAD 3D.  | Média | Estrutura | Obrigatório para documentação do projeto conceitual de estruturas (AP5).  |
| 8 | Construção do Labirinto de Testes Local   |  Projetar e montar uma estrutura de pista de testes 4X4 células (72 X 72 cm²) com paredes brancas (5cm de altura) e topos vermelhos.  | Média | Estrutura | Exigência para validação local e teste de integração do grupo antes da entrega final.   | |

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


