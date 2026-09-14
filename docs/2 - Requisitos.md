# Requisitos

## ESTRURAS



## ENERGIA

# Requisitos Funcionais

| ID | Nome do Requisito | Descrição | Prioridade | Responsável |
|---|---|---|---|---|
| RF01 | Alimentação autônoma embarcada | O sistema de energia deve alimentar todos os subsistemas embarcados (microcontrolador, sensores, motores e módulo de comunicação) a partir de uma fonte transportada pelo próprio Micromouse, sem cabos ou fontes externas. | — | Energia |
| RF02 | Regulação das tensões de operação | O sistema de energia deve fornecer, a partir da fonte, os níveis de tensão exigidos por cada subsistema (lógica do microcontrolador, sensores e drivers dos motores). | — | Energia |
| RF03 | Compatibilidade com Múltiplas Fontes de Energia | O sistema de energia deve prover um meio de comutação manual para selecionar a fonte de alimentação ativa a partir de entradas de energia alternativas, sem alterações físicas na placa. | — | Energia |
| RF04 | Medição do nível de carga | O sistema de energia deve disponibilizar ao microcontrolador um sinal proporcional à carga restante da fonte, permitindo o cálculo do nível e do consumo de bateria. | — | Energia |
| RF05 | Sinalização do estado de energia | O sistema de energia deve indicar visualmente que o robô está energizado e sinalizar quando a carga atingir o nível mínimo de operação. | Could Have | Energia |
| RF06 | Proteção contra sobrecorrente e curto-circuito | O sistema de energia deve interromper a alimentação em caso de curto-circuito ou corrente acima do limite de projeto, protegendo bateria e componentes. | — | Energia |
| RF07 | Proteção da Bateria LiPo (BMS/Undervoltage) | O sistema elétrico deve contar com circuito de proteção específico para impedir descarga profunda (abaixo da tensão mínima por célula) e sobrecarga da bateria LiPo, prevenindo danos permanentes e risco de incêndio. | Alta (Must Have) | Energia |


# Requisitos Não Funcionais

| ID | Nome do Requisito | Descrição | Prioridade | Responsável |
|---|---|---|---|---|
| RNF01 | Separação entre potência e lógica | O sistema de energia deve isolar a linha de alimentação dos motores da linha de alimentação da lógica, de modo que picos de corrente dos motores não provoquem reinicialização do microcontrolador nem leituras erradas dos sensores. | — | Energia |
| RNF02 | Proteção contra inversão de polaridade | O sistema de energia deve impedir danos aos componentes caso a fonte seja conectada com polaridade invertida. | — | Energia |
| RNF03 | Troca e recarga sem desmontagem | O sistema de energia deve permitir a troca das pilhas ou a recarga da bateria sem desmontar a estrutura do Micromouse nem desconectar a PCB. | — | Energia |
| RNF04 | Autonomia de operação | A fonte de energia deve manter o Micromouse em operação contínua por, no mínimo, os 10 minutos de uma tentativa, com margem para três tentativas consecutivas sem troca ou recarga. | — | Energia |
| RNF05 | Estabilidade da tensão sob carga | A tensão de alimentação da lógica não deve variar além de ±5% do valor nominal durante os picos de corrente dos motores, e o microcontrolador não deve reiniciar em nenhuma manobra. | — | Energia |
| RNF06 | Filtragem e Desacoplamento de Ruído | O circuito deve prever capacitores de desacoplamento e/ou filtros na linha de alimentação dos motores para mitigar ruídos elétricos (EMI) gerados pelo acionamento mecânico. | Alta (Must Have) | Energia |
| RNF07 | Dimensionamento da Capacidade (Cálculo de mAh) | A capacidade nominal da bateria escolhida deve ser justificada por cálculo prévio de consumo máximo e médio de todos os subsistemas, assegurando a autonomia de 10 minutos (com margem de segurança para tentativas extras). | Média (Should Have) | Energia |
| RNF08 | Gerenciamento Térmico de Reguladores | Os componentes de regulação de tensão (step-down/lineares) e os drivers de potência devem possuir margem térmica segura, utilizando dissipadores ou áreas de dissipação na PCB se o cálculo térmico exigir. | Média (Should Have) | Energia |

## ELETRÔNICA



## SOFTWARE

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
| :---: | :---- | :---- | :---: | :---- | :---- |
| RF01 | Visualizar telemetria do robô | O sistema web deve permitir ao usuário visualizar os dados de telemetria do Micromouse durante e após a execução, como velocidade, bateria, tempo de percurso e status| Must Have |Frontend  | Requisito essencial para o acompanhamento da execução do robô. |
| RF02 | Visualizar os Dados da Execução | O sistema deve permitir ao usuário visualizar os principais dados de uma execução após sua finalização, incluindo tempo, velocidade e desempenho.  |  Must Have | Frontend | As informações devem ser apresentadas de forma organizada em ordem cronológica de execução. |
| RF03 | Visualizar histórico de execuções | O sistema deve apresentar para o usuário uma seção na aplicação que forneça o histórico de todas as execuções já realizadas. | Must Have | Frontend | Em ordem decrescente de execução (da última para a primeira). |
| RF04 | Visualizar execução em tempo real | Caso haja uma execução com o status "health-check" ou "running", o sistema deve fornecer ao usuário os dados dessa execução na tela inicial, com o tempo da última atualização. | Should Have | Frontend | Ex.: mensagem "última atualização há 4 segundos". Sistema de polling. |
| RF05 | Visualizar os dados captados no health-check | O sistema deve fornecer ao usuário, durante a execução e após o término da mesma, os dados coletados quando o status for "health-check" em uma área específica, separada do resto dos dados coletados na página da execução específica. | Must Have | Frontend | Conectado ao RNF03. |
| RF06 | Fornecer dados de uma execução em tempo real | O sistema deve ser capaz de consumir os dados de telemetria de uma execução em status "health-check" ou "running". | Should Have | Backend |  |
| RF07 | Consulta ao histórico de execuções | O sistema deve permitir ao usuário consultar as execuções anteriores do Micromouse, apresentando os dados registrados de cada tentativa para possibilitar a análise do desempenho. | Must Have  | Backend  | O histórico deve apresentar as informações de cada execução de forma organizada. |
| RF08 | Receber telemetria do micromouse  | O sistema deve receber os dados de telemetria enviados pelo micromouse durante a corrida, contendo no mínimo a célula atual, o nível de bateria e o instante de envio (timestamp).  | Must Have | Backend  | Slide 11\. Protocolo (ex.: MQTT, WebSocket ou HTTP) e formato da mensagem (ex.: JSON) a definir com a equipe de hardware |
| RF09 | Registrar trajeto no labirinto  | O sistema deve armazenar, em ordem cronológica, a sequência de células visitadas pelo micromouse por execução, permitindo reconstruir o trajeto completo.   | Must Have | Backend  | Slide 11\. Sugestão de representação: coluna \+ linha (ex.: A1, B2), igual aos exemplos do enunciado.  |
| RF10 | Registrar início de corrida | O sistema deve criar um registro de corrida no momento em que ela começa, associando o tipo de labirinto (4x4, 8x4 ou 12x4), o número da tentativa e a data/hora de início.  | Must Have | Backend  | Slides 5, 11 e 23\. Definir se o início é disparado pela primeira mensagem do micromouse ou pelo operador na interface web.  |
| RF11 | Calcular consumo de bateria  | O sistema deve calcular o consumo de bateria da corrida a partir das leituras recebidas, informando o nível atual e o total consumido desde o início.  | Must Have | Backend  | Slide 11\. Unidade (% ou mAh) depende do sensor escolhido pela equipe de energia.  |
| RF12 | Calcular velocidade média  | O sistema deve calcular a velocidade média do micromouse, dividindo a distância percorrida pelo tempo decorrido.  | Must Have | Backend  | Slide 11\. Distância estimada por nº de células percorridas × 18 cm (slide 9\) ou obtida dos encoders, se o hardware enviar.  |
| RF13 | Calcular tempo de conclusão  | O sistema deve calcular o tempo total da corrida, do início até a chegada à área de objetivo.  | Must Have | Backend  | Slide 11\. Usar uma única fonte de relógio (micromouse ou servidor) para evitar erros de sincronização.  |
| RF14 | Calcular comparação de tempo gasto entre execuções | O sistema deve calcular, a partir do melhor tempo já executado em um caminho completo, uma comparação em porcentagem do quanto melhor foi o tempo gasto na última execução. | Should Have | Backend | Ajudará na análise sobre a evolução do Micromouse a cada execução, já que há um limite de tempo para a prova. |
| RF15 | Registrar informações sobre a falha de uma execução | O sistema deve registrar nos dados de uma execução em qual célula ocorreu uma falha no trajeto e o motivo da falha. | Must Have | Backend | Ex.: failed: collision / time_exceeded / stuck / out_of_track. |
| RF16 | Registrar os status de uma execução | O sistema deve registrar nos dados de telemetria o status de uma execução, podendo ser: "health-check", "running", "success", "failed". | Must Have | Backend | "Health-check": não começou o trajeto, mas já iniciou a execução. "Running": iniciou o trajeto. "Success": executou o trajeto até o objetivo. "Failed": executou o trajeto, mas não chegou até o objetivo. |
| RF17 | Garantir unicidade entre as execuções | O sistema deve registrar cada execução, independente do seu status, com um número identificador único. | Must Have | Backend | Conexão com a integridade dos dados RNF01. |
| RF18 | Registrar uma única execução por vez | O sistema não deve registrar os dados de uma nova execução caso ainda haja uma em andamento (status diferente de "success" ou "failed"). | Must Have | Backend | Garantir a integridade dos dados advindos de uma execução. |
| RF19 | Enviar feedback de execução recusada | O sistema deve informar de forma clara quando uma execução solicitada foi negada, registrando o evento como uma tentativa rejeitada. | Must Have | Backend | |
| RF20 | Detectar paredes e obstáculos | O sistema deve detectar paredes/obstáculos ao redor do Micromouse em tempo real. | Must Have | Firmware | Pré-requisito para a navegação autônoma. Relação com o sensoriamento da Eletrônica a alinhar com aquela equipe. |
| RF21 | Controlar a movimentação do Micromouse | O sistema deve controlar a movimentação do Micromouse (avançar, curvar, parar). | Must Have | Firmware | |
| RF22 | Calcular velocidade do Micromouse | O sistema deve calcular a velocidade do Micromouse a partir dos dados de movimento disponíveis. | Must Have | Firmware | Campo de telemetria exigido. |
| RF23 | Monitorar bateria | O sistema deve monitorar o nível/consumo de bateria. | Must Have | Firmware | Campo de telemetria exigido; evita parada sem aviso durante a corrida. |
| RF24 | Transmitir telemetria para o sistema web | O sistema deve transmitir os dados de telemetria (bateria, velocidade, trajeto) para o sistema web. | Must Have | Firmware | Envio obrigatório especificado; ausência penaliza a nota em 25%. Protocolo/formato a definir. |
| RF25 | Sinalizar estado do robô | O sistema deve sinalizar visual e sonoramente o estado do robô (início, erro/colisão, conclusão). | Could Have | Firmware | Apoia depuração e acompanhamento da corrida pela equipe e pela banca. |
| RF26 | Detectar chegada ao objetivo | O sistema deve detectar quando o Micromouse alcançou a área de objetivo. | Must Have | Firmware | Sustenta o critério de aceite e o campo de telemetria "desafio cumprido (S/N)". |
| RF27 | Configurar modo de operação | O sistema deve permitir configurar o modo de operação do robô sem necessidade de reprogramação. | Could Have | Firmware | Facilita reaproveitar tentativas anteriores; forma de configuração a definir. |
| RF28 | Rastrear localização e trajeto no labirinto | O sistema deve monitorar a localização do Micromouse e registrar a sequência de células percorridas no labirinto. | Must Have | Firmware | Relacionado ao monitoramento da localização e ao mapeamento do labirinto. |
| RF29 | Realizar um health-check antes de uma execução | O sistema deve realizar um health-check que busca garantir que todos os componentes essenciais para uma execução estejam de acordo, para evitar falhas no meio do processo. | Should Have | Firmware | Verificar funcionamento adequado entre software e hardware, nível de bateria e mobilidade das rodas para curvas. |
| RF30 | Encerrar execução em andamento | O sistema deve permitir que o operador encerre uma execução em andamento, registrando-a como "failed" com o motivo informado. | Must Have | Backend | Sem este requisito, uma execução travada em "running" impede o início da próxima tentativa pelo RF18. |
| RF31 | Detectar perda de comunicação | O sistema deve detectar a ausência de mensagens de telemetria por um intervalo configurável e sinalizar a execução como sem comunicação. | Must Have | Backend | Permite diferenciar "Micromouse parado" de "conexão perdida". Sustenta o RF04 do frontend. |
| RF32 | Detectar estouro do tempo limite | O sistema deve monitorar a duração da execução e encerrá-la como "failed" com motivo `time_exceeded` ao ultrapassar o limite de 10 minutos. | Must Have | Backend | Relacionado aos critérios de tempo máximo da prova. |
| RF33 | Armazenar os dados finais da execução | Ao término da execução, o sistema deve gravar em banco de dados o tipo do labirinto, o trajeto, o consumo de bateria, a velocidade média, o tempo de conclusão, o status, o motivo da falha (quando houver), o número da tentativa e a data/hora. | Must Have | Backend | Exige armazenamento em banco de dados para consultas. |
| RF34 | Consultar execuções de um labirinto | O sistema deve permitir consultar as execuções de um labirinto escolhido pelo usuário, exibindo os dados específicos daquele labirinto. | Must Have | Backend | |
| RF35 | Consultar execuções de todos os labirintos | O sistema deve permitir consultar as execuções de todos os labirintos em uma única exibição. | Must Have | Backend | |
| RF36 | Consultar detalhes de uma execução | O sistema deve permitir consultar todos os dados de uma execução específica, incluindo o trajeto completo, para reexibição do percurso. | Must Have | Backend | Sustenta o RF02 e o RF06 do frontend. |
| RF37 | Registrar paredes detectadas | O sistema deve receber e armazenar as paredes detectadas pelo Micromouse em cada célula, permitindo exibir o mapa descoberto do labirinto. | Could Have | Backend | Não exigido na telemetria, mas o mapeamento é requisito do robô e enriquece a visualização do trajeto. |

# Requisitos Não Funcionais

| ID | Nome do Requisito | Descrição | Prioridade | Responsável | Observações |
|---|---|---|---|---|---|
| RNF01 | Integridade dos dados | A aplicação web deve apresentar os dados de telemetria recebidos sem alterações ou arredondamentos que comprometam sua interpretação, mantendo a unidade de medida do sistema. | Must Have | Frontend | Fundamental para evitar que o usuário interprete dados incorretos durante a execução. |
| RNF02 | Tempo de resposta da interface | A interface web deve apresentar os novos dados de telemetria em até 2 segundos após o recebimento das informações do sistema. | Must Have | Frontend | Requisito de desempenho para visualização da telemetria. |
| RNF03 | Arquitetura das informações | A interface web deve possuir uma arquitetura das informações de uma execução organizada de forma simples e objetiva, a fim de ajudar na rapidez do entendimento e não estressar o usuário com informações não prioritárias. | Must Have | Frontend | Nenhuma informação será omitida, apenas melhor organizada analisando o que precisará ser visto com mais frequência. |
| RNF04 | Latência no acompanhamento em tempo real | Durante uma execução em tempo real, a última atualização dos dados não deverá passar de 3s-5s para garantir que a equipe consiga acompanhar com melhor fidelidade os dados da execução. | Should Have | Backend | Alinhar com o RNF02 do frontend (2 s), para que o atraso total fim a fim fique definido. |
| RNF05 | Persistência dos dados | Os dados das execuções devem permanecer disponíveis após a reinicialização do servidor ou do computador que o hospeda. | Must Have | Backend | |
| RNF06 | Não interferência na corrida | Durante a execução, o sistema não deve enviar comandos ao Micromouse nem alterar seu código ou sua memória sobre o labirinto; a comunicação deve ser apenas de recebimento. | Must Have | Backend | |
| RNF07 | Independência da execução | Uma falha no backend ou na rede não deve interromper a execução do Micromouse, que deve continuar resolvendo o labirinto de forma autônoma. | Must Have | Backend | |
| RNF08 | Tolerância a perda de conexão | Em caso de queda de comunicação, o sistema deve aceitar a reconexão do Micromouse e manter os dados da execução já recebidos. | Must Have | Backend | |
| RNF09 | Validação dos dados recebidos | O sistema deve validar as mensagens de telemetria recebidas (formato, campos obrigatórios e faixas válidas, como célula dentro dos limites do labirinto) e descartar as inválidas sem interromper o serviço. | Must Have | Backend | Evita que uma leitura corrompida derrube o sistema durante a apresentação. |
| RNF10 | Funcionamento em rede local | O sistema deve funcionar em rede local, sem depender de acesso à internet, no local definido pelos professores. | Should Have | Backend | |
| RNF11 | Capacidade de processamento da telemetria | O sistema deve processar pelo menos 10 mensagens de telemetria por segundo sem atraso acumulado. | Should Have | Backend | |
| RNF12 | Desenvolvimento próprio | O backend deve ser desenvolvido pela equipe, sem uso de plataformas prontas de telemetria ou dashboard. | Must Have | Backend | |
| RNF13 | Documentação da API | Os endpoints e o formato das mensagens de telemetria devem estar documentados no repositório do projeto. | Should Have | Backend | |
