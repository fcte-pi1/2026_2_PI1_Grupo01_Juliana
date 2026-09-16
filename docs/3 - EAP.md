# Estrutura Analítica de Produto

| **ID** | **Componente** | **Descrição** | **Dados Técnicos** | **Comentários** |
|:------:|----------------|---------------|--------------------|-----------------|
| 1 | **Sub-sistema: Estrutura** | | | |
| 1.1 | Chassi | | | |
| 1.2 | Suporte | | | |
| 1.3 | Carenagem | | | |
| 1.4 | Atuadores | | | |
| 1.5 | Transmissão | | | |
| 1.6 | Rodas/Hélices | | | |
| 2 | **Sub-sistema: Fonte Energética** | Conjunto responsável por armazenar, converter, proteger, distribuir e monitorar a energia que alimenta todos os demais subsistemas embarcados do Micromouse. | Autonomia mínima de 30 min de operação contínua; massa e volume dentro do envelope de 16,5 × 16,5 cm | Atende aos requisitos 1 a 10 da frente de Energia. |
| 2.1 | Alimentação | Fonte primária de energia embarcada, transportada pelo próprio robô. Duas opções em avaliação: pack de pilhas AA e bateria LiPo, com seleção manual entre elas. | Pilhas: 4 × AA em série, 6,0 V nominais (alcalinas) ou 4,8 V (NiMH). LiPo: 2S (7,4 V) ou 3S (11,1 V). Capacidade alvo ≥ 1500 mAh | Capacidade obtida do consumo estimado (≈1,8 A médio) × 0,5 h, dividido pelos 80% de capacidade utilizável e acrescido de 30% de margem. Valor a confirmar após a medição real de consumo nos testes de energia (AP12). |
| 2.2 | Eletrônica de Potência | Conversão e adequação da tensão da fonte para os níveis exigidos por cada subsistema, com barramentos separados para potência e lógica. | Barramento lógico regulado (5 V e/ou 3,3 V), corrente ≥ 1 A; barramento de potência dimensionado para o pico dos motores (estimado entre 2,7 A e 4 A) | O barramento lógico não pode compartilhar a linha dos motores, sob pena de reinicialização do microcontrolador. Fronteira com a Eletrônica a alinhar: os drivers dos motores podem ser alocados aqui ou no subsistema de Hardware. |
| 2.3 | Proteções | Elementos que impedem danos ao conjunto em falhas elétricas: sobrecorrente, curto-circuito, inversão de polaridade e descarga profunda da bateria. | Elemento de sobrecorrente dimensionado acima do pico de operação; proteção de polaridade na entrada; corte por subtensão referenciado à tensão mínima por célula do fabricante | Itens de proteção não constam da tabela de requisitos de Energia após a última revisão; confirmar com a professora se permanecem nesta frente ou migram para a Eletrônica. |
| 2.4 | Gerenciamento de Energia | Monitoramento da carga disponível e sinalização do estado de energia do robô ao usuário e ao software embarcado. | Sinal de medição compatível com a faixa de entrada analógica do microcontrolador; indicação visual de robô energizado e de carga mínima | Fornece o dado de telemetria "consumo de bateria" exigido no slide 11. Método de medição ainda em aberto: leitura de tensão, medição de corrente em série ou contagem de carga. |
| 2.5 | Distribuição e Seleção de Fonte | Caminho elétrico entre a fonte e os demais subsistemas: chaveamento geral, seleção da fonte ativa, conectores e cabeamento. | Conectores compatíveis com a corrente de pico; seleção manual de fonte com bloqueio de conexão simultânea; referencial de terra comum entre os barramentos | A seleção deve impedir que as duas fontes fiquem ligadas ao mesmo tempo. Cabeamento e conectores devem suportar o pico de corrente, não apenas a média. |
| 2.6 | Suporte e Fixação da Fonte | Compartimento e fixação mecânica da fonte de energia à estrutura, permitindo instalação e remoção sem ferramentas especiais. | Fixação capaz de resistir a vibração e colisão sem deslocamento; acesso à fonte sem desmontar a estrutura | Viabiliza a troca ou recarga entre tentativas, permitida apenas com o robô em repouso. Interface com a frente de Estruturas quanto ao ponto de fixação e à distribuição de massa. |

 <img src="https://raw.githubusercontent.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/refs/heads/feature/docs-eap-energia/docs/figs/PI1%20Micromouse%20-%20EAP.jpg" alt="" width=""> 

| **ID** | **Componente** | **Descrição** | **Dados Técnicos** | **Comentários** |
|:------:|----------------|---------------|--------------------|-----------------|
| 3 | **Sub-sistema: Hardware** | | | |
| 3.1 | Processamento | | | |
| 3.2 | Sensor 1 | | | |
| 3.3 | Sensor 2 | | | |
| 3.4 | Controle 1 | | | |
| 3.5 | Controle 2 | | | |
| 3.6 | Comunicação 1 | | | |
| 3.7 | Comunicação 2 | | | |


# 4. Sub-sistema: Software

# 4.1 Gerenciamento

| ID | Nome | Descrição |
|---|---|---|
| **4.1** | **Gerenciamento** | Atividades de organização, planejamento e documentação do desenvolvimento do software. |
| 4.1.1 | Requisitos | Definição e organização dos requisitos do software. |
| 4.1.1.1 | Requisitos funcionais | Funções que o software deverá executar. |
| 4.1.1.2 | Requisitos não funcionais | Restrições e características de qualidade do software. |
| 4.1.2 | Planejamento | Organização do desenvolvimento e acompanhamento do projeto. |
| 4.1.2.1 | Backlog | Lista e priorização das funcionalidades e atividades. |
| 4.1.2.2 | Sprints | Divisão do desenvolvimento em ciclos de trabalho. |
| 4.1.2.3 | Cronograma | Planejamento temporal das atividades. |
| 4.1.3 | Documentação | Documentação técnica e de desenvolvimento do software. |
| 4.1.3.1 | Diagrama UML | Representação da arquitetura e estrutura do software. |
| 4.1.3.2 | GitHub | Versionamento, armazenamento e gerenciamento do código-fonte. |

# 4.2.1 Front End

| ID | Nome | Descrição |
|---|---|---|
| **4.2.1** | **Front End** | Interface responsável pela visualização e interação com os dados do Micromouse. |
| 4.2.1.1 | Visualização | Apresentação das informações do sistema ao usuário. |
| 4.2.1.2 | Labirinto | Representação visual do labirinto. |
| 4.2.1.3 | Mapeamento | Visualização do mapa construído pelo robô. |
| 4.2.1.4 | Telemetria | Consumo de dados advindos do robô|
| 4.2.1.5 | Rotas | Visualização da rota planejada ou percorrida pelo robô. |
| 4.2.1.6 | Interface | Interface de interação e acompanhamento do funcionamento do sistema. |

# 4.2.2 Back End

| ID | Nome | Descrição |
|---|---|---|
| **4.2.2** | **Back End** | Camada responsável pelo processamento, comunicação e gerenciamento dos dados. |
| 4.2.2.1 | API | Interface para comunicação entre os componentes do sistema. |
| 4.2.2.2 | Comunicação | Gerenciamento da comunicação entre o sistema e o Micromouse. |
| 4.2.2.3 | Dados | Gerenciamento e processamento dos dados recebidos. |
| 4.2.2.3.1 | Banco | Armazenamento dos dados do sistema. |
| 4.2.2.3.2 | Processamento | Tratamento e processamento dos dados. |

# 4.2.3 Software Embarcado

| ID | Nome | Descrição |
|---|---|---|
| **4.2.3** | **Software Embarcado** | Software executado no sistema embarcado do Micromouse para controlar o robô e realizar a navegação. |
| 4.2.3.1 | Controle | Controle da movimentação e dos componentes do robô. |
| 4.2.3.1.1 | Motores | Controle de velocidade, direção e acionamento dos motores. |
| 4.2.3.1.2 | Sensores | Leitura e processamento das informações dos sensores. |
| 4.2.3.2 | Navegação | Algoritmos responsáveis pela navegação e resolução do labirinto. |
| 4.2.3.2.1 | Flood Fill | Algoritmo utilizado para determinar caminhos e distâncias no labirinto. |
| 4.2.3.2.2 | Mapeamento | Construção e atualização do mapa do labirinto durante a execução. |
| 4.2.3.3 | Telemetria | Gerenciamento das informações enviadas pelo Micromouse para acompanhamento externo. |
| 4.2.3.3.1 | Envio | Envio dos dados de funcionamento, navegação e sensores. |
| 4.2.3.4 | Diagnóstico | Monitoramento do funcionamento do sistema e identificação de possíveis falhas. |
| 4.2.3.5 | Comunicação | Comunicação do software embarcado com os demais componentes do sistema. |

# 4.3 Validação

| ID | Nome | Descrição |
|---|---|---|
| **4.3** | **Validação** | Verificação do funcionamento correto dos componentes e da integração do software. |
| 4.3.1 | Integração | Integração entre os diferentes componentes do sistema. |
| 4.3.1.1 | Frontend–Backend | Integração entre a interface e os serviços do sistema. |
| 4.3.1.2 | Backend–Sistema Embarcado | Integração entre o backend e o sistema embarcado. |
| 4.3.2 | Testes | Testes para verificar o funcionamento e atendimento aos requisitos. |
| 4.3.2.1 | Testes unitários | Verificação individual dos componentes de software. |
| 4.3.2.2 | Testes de navegação | Verificação dos algoritmos de navegação e resolução do labirinto. |
| 4.3.2.3 | Testes de telemetria | Verificação do envio e recebimento das informações de telemetria. |

 <img src="https://github.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/blob/eap-software/docs/figs/eap-software.jpg?raw=true" alt="" width=""> 