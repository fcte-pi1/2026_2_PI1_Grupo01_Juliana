# Orçamento

Mande o **link público** da planilha de orçamento, e exporte em Markdown no formato a seguir:

### Orçamento do Subsistema de Estrutura

| ID | Item / Componente | Especificação Técnica | Qtd | Valor Unit. (R$) | Valor Total (R$) | Origem / Status |
| :-: | :--- | :--- | :-: | :-: | :-: | :--- |
| **1** | Placa de Acrílico / PETG | Impressão 3D Chassi Base 3mm | 1 | R$ 60,00 | R$ 60,00 | A comprar |
| **2** | Caster Ball | Roda Boba Esférica Metálica Ball Caster | 1 | R$ 33,91 | R$ 34,00 | A comprar |
| **3** | Par de Rodas e Pneus | Diâmetro = 34mm | 4 | R$ 12,33 | R$ 50,00 | A comprar |
| **4** | Placa MDF | MDF padrão 50 × 50 × 12 mm | 3 | R$ 28,28 | R$ 85,00 | A comprar |
| **5** | Placa Perfurada | Placa Circuito Dupla Face Ilhada Fibra 5x7cm | 5 | R$ 6,10 | R$ 31,00 | A comprar |
| **6** | Motores DC N20 | Mini Motor Redutor DC 6V N20 750 RPM, com Encoder 6V | 3 | R$ 59,60 | R$ 180,00 | A comprar |
| **—** | **TOTAL ESTIMADO (ESTRUTURA)** | — | — | — | **R$ 440,00** | — |

<font size="2"><p style="text-align: center">Fonte: Jefferson de Souza Reis, Geovana (Bygeo57), Júlio César (DeNNis715) e Murilo (MuriloPi13), 2026.</p></font>


### Orçamento do Subsistema de Energia

| ID | Item / Componente | Especificação Técnica | Qtd | Valor Unit. (R$) | Valor Total (R$) | Origem / Status |
| :-: | :--- | :--- | :-: | :-: | :-: | :--- |
| **1** | Bateria LiPo 2S 7.4V | 850mAh - 1000mAh (mín. 25C de taxa de descarga) | 2 | R$ 45,00 | R$ 90,00 | A comprar |
| **2** | Carregador/Balanceador LiPo | Carregador B3 Compact ou IMAX B6 com balanceamento | 1 | R$ 35,00 | R$ 35,00 | A comprar |
| **3** | Módulo Conversor Buck-Boost DC-DC | Regulador chaveado tipo XL6009 / TPS63020 (Saída 5V estável) | 2 | R$ 15,00 | R$ 30,00 | A comprar |
| **4** | Regulador de Tensão LDO 3.3V | CI LDO AMS1117-3.3V ou SPX3819 | 4 | R$ 2,50 | R$ 10,00 | A comprar |
| **5** | Conectores XT60 (Macho/Fêmea) | Pares de conectores com tubos termorretráteis para bateria LiPo | 4 | R$ 4,00 | R$ 16,00 | A comprar |
| **6** | Suporte para 4 Pilhas AA | Suporte em plástico ABS com fios (alimentação de testes) | 1 | R$ 6,00 | R$ 6,00 | Adquirido / Bancada |
| **7** | Pilhas Alcalinas AA | Kit com 4 pilhas 1.5V para alimentação de testes em bancada | 1 | R$ 14,00 | R$ 14,00 | Adquirido / Bancada |
| **8** | Chave Gangorra / Interruptor Liga/Desliga | Chave KCD1 2 posições (mín. 10A) para corte geral | 2 | R$ 3,00 | R$ 6,00 | A comprar |
| **9** | Fusível Rearmável PTC (PolySwitch) | Corrente de Hold ~2A / Trip ~3.5A para proteção do circuito | 4 | R$ 2,50 | R$ 10,00 | A comprar |
| **10** | Diodo Schottky de Potência | Diodo 1N5822 (3A) para proteção de inversão de polaridade | 4 | R$ 1,50 | R$ 6,00 | A comprar |
| **11** | Resistores de Precisão 1% | Kit de resistores (10kΩ e 47kΩ) para divisor de tensão do ADC | 10 | R$ 0,50 | R$ 5,00 | Disponível no Lab |
| **12** | Capacitores de Desacoplamento | Capacitores eletrolíticos (100µF) e cerâmicos (100nF) | 10 | R$ 0,80 | R$ 8,00 | Disponível no Lab |
| **13** | Conectores JST-XH e Barra de Pinos | Pinos de 2.54mm e conectores para barramentos de alimentação | 2 | R$ 5,00 | R$ 10,00 | A comprar |
| **14** | Kit Tubos Termorretráteis | Diâmetros variados para isolamento elétrico das conexões | 1 | R$ 12,00 | R$ 12,00 | Disponível no Lab |
| **15** | Cabo de Silicone Flexível AWG 20 | Fios vermelho/preto de silicone de alta flexibilidade (2m) | 2 | R$ 6,00 | R$ 12,00 | A comprar |
| **16** | Placa de Fenolite Ilhada (Protótipo) | Placa ilhada 5x7cm para montagem do módulo de alimentação | 2 | R$ 5,00 | R$ 10,00 | A comprar |
| **—** | **TOTAL ESTIMADO (ENERGIA)** | — | — | — | **R$ 280,00** | — |

### Orçamento do Subsistema de Eletrônica

| Componente | Uso no Robô | Qtd. Seg. (Reserva) | Qtd. Total | Preço Unit. Médio (R$) | Subtotal Estimado (R$) | Justificativa da Reserva | Origem / Status |
| :--- | :-: | :-: | :-: | :-: | :-: | :--- | :--- |
| Microcontrolador ESP32 DevKit V1 (30 pinos) | 1 | 1 | 2 | 36,88 | 73,66 | Curtos acidentais nos pinos GPIO ou falha de regulador interno. | 1 disponível / A comprar 1 |
| Sensores ToF (VL53L0X) | 4 | 2 | 6 | 29,81 | 180,00 | Módulos I²C são sensíveis a estática (ESD) e falhas nos pinos XSHUT. | A comprar |
| Ponte H Dupla (TB6612FNG) | 1 | 1 | 2 | 34,00 | 68,00 | Drivers queimam facilmente por picos de corrente (stall) dos motores. | A comprar |
| Módulo Regulador Buck-Boost (5V) | 1 | 1 | 2 | 25,00 | 50,00 | Essencial para não perder o projeto inteiro se houver pico na bateria. | A comprar |
| Diodo Schottky SS34 (SMD ou PTH) | 1 | 4 | 5 | 2,50 | 12,50 | Proteção reversa da bateria. Queima "para salvar" o restante do circuito. | A comprar |
| Chave DIP Switch (4 vias) | 1 | 1 | 2 | 20,00 | 40,00 | Quebra mecânica das chaves. | A comprar |
| Buzzer Ativo (3,3 V) | 1 | 1 | 2 | 10,00 | 20,00 | Componente muito barato para arriscar ficar sem diagnóstico sonoro. | A comprar |
| Transistor NPN (P2N2222A) | 1 | 4 | 5 | 2,00 | 10,00 | Acionamento do buzzer. Terminais muito frágeis. | A comprar |
| Kit Componentes Passivos (Resistores 10k, 22k, 1k, Caps 100nF e LEDs) | - | - | 1 kit | 35,00 | 35,00 | Utilizados no divisor de tensão (ADC), filtros e interface. | A comprar |
| Insumos (Estanho, Fios AWG flexíveis, Barras de Pinos e Conectores JST) | - | - | 1 kit | 40,00 | 40,00 | Consumo natural de laboratório para prototipagem e solda da PCB. | A comprar |
| **TOTAL ESTIMADO** | | | | | **R$ 529,16** | | — |


<font size="2"><p style="text-align: center">Fonte: [Ana Victória](https://github.com/navicg), [João Vitor](https://github.com/Jauzimm), [Marcus](https://github.com/MarcusVRezende), [Anderson](https://github.com/leicamAnd), [Juan](https://github.com/IndianoDev) e [Fábio](https://github.com/fabiofonteles1), 2026.</p></font>

### Orçamento do Subsistema de Software

O subsistema de software **não prevê aquisição de hardware dedicado**: o Micromouse, sensores, motores e ESP32 constam no orçamento de **Eletrônica/Hardware** e de **Estrutura**; a telemetria usa o **Bluetooth SPP integrado da ESP32** (sem módulo HM-10 adicional). A operação em **rede local** (RNF10) dispensa hospedagem em nuvem. Ferramentas de desenvolvimento, CI e banco em Docker são **open source** ou **gratuitas** (GitHub da disciplina).

| ID | Item / Serviço | Especificação / Observação | Qtd | Valor Unit. (R$) | Valor Total (R$) | Origem / Status |
| :-: | :--- | :--- | :-: | :-: | :-: | :--- |
| **1** | Notebook operacional | Receptor da ponte serial/Bluetooth, backend, front-end e demonstração na APT (equipe) | 1 | R$ 0,00 | R$ 0,00 | Já disponível (equipe) |
| **2** | Stack de desenvolvimento | Python (FastAPI), Node (React/Vite), PlatformIO, PostgreSQL via Docker, GitHub Actions | 1 | R$ 0,00 | R$ 0,00 | Open source / GitHub Education |
| **3** | Repositório e documentação | MkDocs, issues e GitHub Projects (46 tarefas de software) | 1 | R$ 0,00 | R$ 0,00 | Repositório do grupo |
| **4** | Cabo USB (firmware) | Gravação e telemetria de bancada na ESP32 (desenvolvimento) | 1 | R$ 15,00 | R$ 15,00 | A comprar / compartilhado com Eletrônica |
| **5** | Adaptador USB–serial (opcional) | Apoio à ponte serial em bancada, se o notebook não expuser porta COM nativa | 0 | R$ 25,00 | R$ 0,00 | Opcional — não previsto na v1 |
| **6** | Hospedagem em nuvem | Não aplicável: sistema previsto para execução em **rede local** no local da prova | — | — | R$ 0,00 | Fora do escopo TAP |
| **—** | **TOTAL ESTIMADO (SOFTWARE)** | Custos incrementais ao hardware já orçado nas outras frentes | — | — | **R$ 15,00** | — |

> **Rateio:** o item 4 pode ser absorvido pelo orçamento de Eletrônica (bancada ESP32). Se a equipe já possuir cabo, o **total de Software permanece R$ 0,00**.

<font size="2"><p style="text-align: center">Fonte: [Amanda de Moura](https://github.com/AmandaaMoura), 2026. Gerência de Software — PI1 Grupo 01.</p></font>

## Orçamento Total do Projeto

| Subsistema | Valor Estimado (R$) |
|---|---:|
| Energia | R$ 280,00 |
| Eletrônica | R$ 529,16 |
| Software | R$ 15,00 |
| Estrutura | R$ 440,00 |
| **TOTAL GERAL** | **R$ 1.264,16** |