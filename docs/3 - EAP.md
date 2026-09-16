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
| 3 | **Sub-sistema: Hardware** | | | |
| 3.1 | Processamento | | | |
| 3.2 | Sensor 1 | | | |
| 3.3 | Sensor 2 | | | |
| 3.4 | Controle 1 | | | |
| 3.5 | Controle 2 | | | |
| 3.6 | Comunicação 1 | | | |
| 3.7 | Comunicação 2 | | | |
| 4 | **Sub-sistema: Software** | | | |
| 4.1 | Controle | | | |
| 4.2 | Navegação | | | |
| 4.3 | Interface | | | |
| 4.4 | Diagnóstico | | | |
| 4.5 | | | | |
| 4.6 | | | | |
| 4.7 | | | | |
