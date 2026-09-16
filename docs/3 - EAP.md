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
| 2 | **Sub-sistema: Fonte Energética** | | | |
| 2.1 | Alimentação | | | |
| 2.2 | Eletrônica de Potência | | | |
| 2.3 | Proteções | | | |
| 2.4 | Gerenciamento de Energia | | | |
| 2.5 | | | | |
| 2.6 | | | | |
| 3 | **Sub-sistema: Eletrônica/Hardware** | Sistema eletrônico embarcado do Micromouse Ratatouille, responsável pelo processamento, sensoriamento, acionamento e comunicação dos dados de navegação. | PCB própria de até 12 cm × 12 cm; conjunto montado dentro do limite de 16,5 cm de comprimento e largura do robô. | Integração com Estrutura, Energia e Software. Operação durante os 10 minutos de cada desafio sem falhas elétricas não planejadas (RNF1, RNF3 e RNF4). |
| 3.1 | **Unidade de processamento** | Núcleo eletrônico que executa o firmware e interliga sensores, motores e telemetria. | Microcontrolador, interfaces de entrada/saída e recurso de recuperação de travamentos. | Modelo a definir no projeto conceitual conforme a quantidade de pinos, as interfaces e os níveis elétricos necessários. |
| 3.1.1 | Microcontrolador | Componente responsável pela leitura dos sensores e geração dos sinais de controle e comunicação. | Entradas digitais/analógicas para sensores, bateria e DIP switch; saídas para drivers, LED e buzzer; interface para Bluetooth, sem multiplexação adicional. | Mapa de pinos e compatibilidade elétrica documentados; leitura dos sensores, acionamento dos motores e transmissão de telemetria funcionando simultaneamente durante 10 minutos, sem travamentos ou reinicializações não planejadas (RF1–RF6, RF9, RNF1 e RNF9). |
| 3.1.2 | Recurso de watchdog | Circuito ou função de supervisão para recuperação do microcontrolador em caso de travamento do firmware. | Watchdog interno ou externo, com configuração e tempo de atuação definidos com a equipe de Firmware. | Recuperação verificada por travamento induzido em bancada; comportamento após reinicialização documentado (RNF6). |
| 3.2 | **Conjunto de sensoriamento** | Componentes e circuitos que disponibilizam informações do ambiente e da bateria ao microcontrolador. | Sensores infravermelhos, condicionamento de sinais, proteção de entradas e interface de leitura da bateria. | Montagem alinhada com Estrutura e grandeza de bateria alinhada com Energia (RF1, RF3 e RF8). |
| 3.2.1 | Sensores infravermelhos de paredes | Sensores para detecção de paredes à esquerda, à direita e à frente do robô. | Previsão do TAP: 1 sensor que será posiiconado mais à frente da estrutura, abrangendo 180 graus. Referências preliminares HW-201 e módulo identificado como LM393. | Detecção a validar em células de 18 cm, paredes de 5 cm de altura e 1,2 cm de espessura, brancas com topo vermelho e piso preto (RF1). |
| 3.2.2 | Circuitos de condicionamento e proteção dos sensores | Circuitos que adequam os sinais dos sensores às entradas do microcontrolador. | Filtragem, amplificação quando necessária, ajuste de limiar e proteção contra picos de tensão e descargas eletrostáticas; valores a dimensionar. | Leituras estáveis e níveis elétricos compatíveis. A resposta às superfícies da pista deve ser verificada por calibração e testes (RF8 e RNF5). |
| 3.2.3 | Interface de leitura da bateria | Circuito de aquisição do sinal de bateria para monitoramento e telemetria. | Divisor de tensão ligado ao conversor analógico-digital (ADC), dimensionado conforme a tensão máxima da fonte e a faixa de entrada do microcontrolador. | Leituras comparadas com instrumento de referência. Energia define a fonte e a grandeza disponível; Firmware trata os dados para estimativa de carga (RF3; Energia RF04; Software RF23). |
| 3.3 | **Conjunto de acionamento dos motores** | Eletrônica responsável pelo acionamento independente dos motores de passo e pelas interfaces usadas na estimativa de movimento. | Dois canais de acionamento, compatíveis com os motores e com a alimentação fornecida por Energia. | Avanço, curvas e paradas verificados em conjunto com Firmware e Estrutura (RF2). |
| 3.3.1 | Drivers dos motores de passo | Circuitos dedicados ao fornecimento de corrente e à sequência de acionamento das fases de cada motor. | Um canal por motor; corrente e tensão compatíveis com os motores previstos no TAP como Nema 11; proteção contra sobrecorrente nos enrolamentos. | Modelo e dissipação a dimensionar pelas especificações dos motores escolhidos; funcionamento dentro dos limites elétricos e térmicos dos componentes (RF2 e RF7). |
| 3.3.2 | Interface STEP/DIR | Conexões dos sinais de passo e direção entre microcontrolador e drivers. | Linhas independentes para os dois motores, com níveis lógicos compatíveis e pinagem documentada. | Firmware utiliza os pulsos enviados para estimar deslocamento e velocidade em malha aberta; esta interface não mede a rotação real das rodas (RF6; Software RF21 e RF22). |
| 3.4 | **Comunicação e interface local** | Componentes de transmissão de telemetria, configuração prévia e sinalização do estado do robô. | Módulo Bluetooth, DIP switch de quatro vias, LED, buzzer e circuitos de acionamento. | Interfaces e comportamento definidos em conjunto com Software. |
| 3.4.1 | Módulo Bluetooth de telemetria | Enlace sem fio para envio dos dados do microcontrolador ao dispositivo receptor integrado ao sistema web. | HM-10 BLE 4.0 como referência preliminar do TAP; módulo, alimentação, interface com o microcontrolador e receptor a confirmar. | Envio de dados reais validado de ponta a ponta. Protocolo, formato e conexão do receptor ao backend definidos com Software; navegação autônoma mesmo sem comunicação (RF4; Software RF24 e RNF07). |
| 3.4.2 | DIP switch de configuração | Interface física para seleção do modo de operação sem reprogramação. | Quatro vias conectadas a entradas digitais com níveis lógicos definidos; tabela de combinações a documentar com Firmware. | Configuração lida antes da corrida, respeitando a restrição de intervenção durante o percurso (RF9; Software RF27). Prioridade Should Have na Eletrônica. |
| 3.4.3 | Indicadores visuais e sonoros | LED e buzzer para indicação de início, erro/colisão e conclusão do percurso. | Resistor limitador e/ou estágio de chaveamento dimensionado para a corrente dos indicadores e os limites das saídas do microcontrolador. | Estados acionados e identificáveis durante os testes com Firmware (RF5 e RF10; Software RF25). Prioridade Should Have na Eletrônica. |
| 3.5 | **Placa e interconexões** | Base física e elétrica de integração dos componentes embarcados. | Placa perfurada para personzalização por soldagem. | Disposição compacta, fixação segura e acesso para manutenção em conjunto com Estrutura (RNF3 e RNF4). |
| 3.5.1 | Conectores e cabeamento | Interligações entre placa, sensores, motores e fonte de energia. | Conectores padronizados, polaridade e pinagem identificadas; JST de quatro vias como referência preliminar para os motores. | Conexões firmes e acessíveis, com prevenção de inversão e sem mau contato durante o movimento (RNF1, RNF3 e RNF7). |
| 3.5.2 | Interface de alimentação da eletrônica | Conexões de entrada e distribuição local das tensões fornecidas por Energia aos componentes eletrônicos. | Barramentos compatíveis com lógica, sensores e drivers; desacoplamento local e organização das conexões para reduzir interferências. | Tensões nos componentes dentro das faixas especificadas, inclusive no acionamento dos motores; regulação, fonte e proteções de potência definidas por Energia (RNF1 e RNF2). |
| 3.6 | **Documentação técnica do hardware** | Conjunto de arquivos que descreve a eletrônica e permite reproduzir a montagem do produto. | Diagrama de blocos, esquemático elétrico, pinagem, diagrama de conexões, layout da PCB e relação de componentes. | Arquivos de hardware versionados em `hw/` e descrição no projeto conceitual de hardware; evidências de validação no documento de testes de hardware (RNF8). |
| 3.6.1 | Diagramas e esquemático elétrico | Representação dos blocos funcionais e das ligações elétricas do hardware. | Diagrama de blocos, símbolos e identificação dos componentes, alimentação, pinagem e conexões com sensores, drivers e módulo Bluetooth. | Correspondência entre esquemático, mapa de pinos e montagem; interfaces com Energia e Firmware identificadas (RNF8). |
| 3.6.2 | Layout da Placa Perfurada | Arquivo de projeto com a disposição dos componentes e o roteamento das trilhas da placa própria. | Contorno de até 12 cm × 12 cm; posições de conectores e fixações; trilhas de potência e sinais organizadas para reduzir interferência nas leituras. | Dimensões e montagem compatíveis com Estrutura; ligações coerentes com o esquemático; verificação de regras elétricas e de layout documentada, com eventuais exceções justificadas (RNF1, RNF3, RNF4 e RNF8). |
| 3.6.3 | Relação de componentes e compatibilidade elétrica | Registro dos componentes selecionados, incluindo microcontrolador, drivers e demais circuitos integrados da placa. | Referência no esquemático, modelo, quantidade, função, tensão de operação, corrente de consumo ou de acionamento e níveis de sinal aplicáveis; folhas de dados dos fabricantes como suporte. | Componentes identificados e compatíveis entre si e com a alimentação; limites elétricos e térmicos registrados para orientar montagem, orçamento e testes (RF7, RNF2, RNF8 e RNF9). |
| 4 | **Sub-sistema: Software** | | | |
| 4.1 | Controle | | | |
| 4.2 | Navegação | | | |
| 4.3 | Interface | | | |
| 4.4 | Diagnóstico | | | |
| 4.5 | | | | |
| 4.6 | | | | |
| 4.7 | | | | |

## Notas da EAP de Eletrônica/Hardware

Os identificadores RF e RNF nas linhas do subsistema 3 referem-se à seção **ELETRÔNICA** do documento [2 - Requisitos](<2 - Requisitos.md>), exceto quando outra frente é indicada. Os itens representam componentes e entregas do produto; atividades, prazos e atribuições individuais serão detalhados no cronograma e nas issues.

### Interfaces e definições pendentes

- **Componentes:** os modelos citados no [TAP](<1 - TAP.md>) são referências de orçamento. A seleção final do microcontrolador, dos drivers, dos sensores e do módulo Bluetooth depende do dimensionamento e da compatibilidade definidos no [projeto conceitual de hardware](<4.3 - Projeto conceitual de hardware.md>). O orçamento menciona um conector Bluetooth com módulo não incluído; a disponibilidade do módulo necessário ao RF4 deve ser confirmada.
- **Sensoriamento:** os requisitos mencionam sensores de linha/cor e distinção entre superfícies, mas o tema da disciplina define o objetivo pela posição no labirinto, sem especificar uma marca de chegada. A necessidade de um sensor adicional e o atendimento do RF8 pelos sensores previstos devem ser esclarecidos no projeto conceitual, sem presumir que o conjunto já realiza reconhecimento de cores.
- **Movimento:** a EAP segue os motores de passo e a interface STEP/DIR previstos nos requisitos. A estimativa por pulsos depende da execução física dos passos e deverá ser confrontada com o deslocamento observado nos testes.
- **Energia e telemetria:** a Eletrônica integra a leitura da bateria; Energia define fonte, regulação e grandeza disponível, e Software define a conversão e apresentação dos dados. Unidade e método de estimativa de consumo precisam ser acordados entre as frentes.
- **Integração:** Estrutura define suportes, rodas e montagem dos motores; Eletrônica fornece os circuitos de acionamento e as conexões. Firmware implementa leitura, controle e navegação, enquanto o sistema web recebe e armazena a telemetria. O enlace Bluetooth precisa de um receptor e de integração com o backend, a definir entre Eletrônica e Software.
- **Validação:** os critérios descritos na tabela orientam os [testes de hardware](<7.3 - Testes de hardware.md>) e os [testes de integração](<7.5 - Testes de integração.md>); não representam resultados já obtidos. A confiabilidade deve ser verificada durante os 10 minutos do desafio; o requisito de autonomia de 30 minutos pertence à frente de Energia.

### Contribuições da EAP de referência

A [EAP do Grupo 01 Hilmer](https://fcte-pi1.github.io/2026_1_PI1_Grupo01_Hilmer/recursos/eap.html) destaca o layout da PCB, os circuitos integrados e os critérios de funcionamento dos módulos. Essa organização orientou o detalhamento dos arquivos de layout e da relação de componentes em 3.6.2 e 3.6.3, além do critério de operação simultânea em 3.1.1. A placa física está em 3.5.1; seus arquivos de projeto estão em 3.6, permitindo identificar cada entrega sem duplicar os componentes.

O projeto de referência também inclui encoders e controle por PWM. A solução descrita nos requisitos do Ratatouille utiliza motores de passo com sinais STEP/DIR e estimativa de movimento pelos pulsos enviados. A inclusão de encoders dependeria de uma revisão dos requisitos, das interfaces e do orçamento da equipe.

### Referências utilizadas na seção de Eletrônica/Hardware

- [1 - Termo de Abertura do Projeto](<1 - TAP.md>): escopo, restrições e orçamento preliminar do Ratatouille.
- [2 - Requisitos](<2 - Requisitos.md>): requisitos de Eletrônica e interfaces com Estrutura, Energia e Software.
- [EAP e Dicionários da EAP — Grupo 01 Hilmer, 2026/1](https://fcte-pi1.github.io/2026_1_PI1_Grupo01_Hilmer/recursos/eap.html): referência de decomposição e descrição dos componentes de eletrônica, adaptada ao escopo do Ratatouille.
- **Tema_PI1 - Google Slides.pdf**, slides 7–13 e 18–23: autonomia, características da pista, telemetria, entregas e avaliação.
- **Fases_Projeto_Planejamento I.pdf**, slides 13–22: decomposição hierárquica orientada a entregas.
- **Fases_Projeto_Planejamento II.pdf**: contexto de planejamento e tratamento de incertezas, considerado nas definições pendentes.
