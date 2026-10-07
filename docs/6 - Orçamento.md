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

Itens alinhados ao [projeto conceitual de energia](4.2%20-%20Projeto%20conceitual%20de%20energia.md) (v1.4). O total soma só o que precisa ser comprado.

| ID | Item / Componente | Especificação Técnica | Qtd | Valor Unit. (R$) | Valor Total (R$) | Origem / Status |
| :-: | :--- | :--- | :-: | :-: | :-: | :--- |
| **1** | Bateria LiPo 2S 7,4 V | 500 mAh, mín. 20C de taxa de descarga (1 em uso + 1 reserva) | 2 | R$ 45,00 | R$ 90,00 | A comprar |
| **2** | Carregador/Balanceador LiPo | Carregador B3 Compact ou IMAX B6 com balanceamento | 1 | R$ 35,00 | R$ 35,00 | A comprar |
| **3** | Regulador de Tensão LDO 3,3 V | CI AMS1117-3.3 ou SPX3819, alimentado pelos 5 V (sensores ToF e encoders) | 2 | R$ 2,50 | R$ 5,00 | A comprar |
| **4** | Conectores XT60 (Macho/Fêmea) | Pares de conectores para as baterias LiPo e a entrada do robô | 4 | R$ 4,00 | R$ 16,00 | A comprar |
| **5** | Chave Gangorra / Interruptor Liga/Desliga | Chave KCD1 2 posições (mín. 10 A) para corte geral | 2 | R$ 3,00 | R$ 6,00 | A comprar |
| **6** | Fusível Rearmável PTC | Bourns MF-MSMF200: hold 2,0 A / trip 3,5 A | 2 | R$ 2,50 | R$ 5,00 | A comprar |
| **—** | **TOTAL A COMPRAR (ENERGIA)** | — | — | — | **R$ 157,00** | — |

**Já disponíveis (fora do total):** suporte para 4 pilhas AA (R$ 6,00) e pilhas alcalinas AA (R$ 14,00), já adquiridos; tubos termorretráteis (R$ 12,00), do laboratório.

**Orçados na Eletrônica (não repetidos aqui):** conversor buck-boost de 5 V, diodo Schottky SS34 de proteção de polaridade, resistores do divisor do ADC (22 kΩ / 10 kΩ), capacitores e insumos (fios, barras de pinos, conectores JST e estanho). O buck-boost precisa aceitar a faixa de entrada das duas fontes, de 4,4 V (pilhas descarregadas) a 8,4 V (LiPo cheia): o XL6009 comum é só elevador (boost) e o TPS63020 aceita no máximo 5,5 V na entrada, então nenhum dos dois serve.

<font size="2"><p style="text-align: center">Fonte: [João Marcos](https://github.com/JJOAOMARCOSS) e [Giovana Martins](https://github.com/Giih-martins), 2026.</p></font>

### Orçamento do Subsistema de Eletrônica

| Componente | Uso no Robô | Qtd. Seg. (Reserva) | Qtd. Total | Preço Unit. Médio (R$) | Subtotal Estimado (R$) | Justificativa da Reserva | Origem / Status |
| :--- | :-: | :-: | :-: | :-: | :-: | :--- | :--- |
| Microcontrolador ESP32 DevKit V1 (30 pinos) | 1 | 1 | 2 | 36,88 | 73,76 | Curtos acidentais nos pinos GPIO ou falha de regulador interno. | 1 disponível / A comprar 1 |
| Sensores ToF (VL53L0X) | 4 | 2 | 6 | 29,81 | 180,00 | Módulos I²C são sensíveis a estática (ESD) e falhas nos pinos XSHUT. | A comprar |
| Ponte H Dupla (TB6612FNG) | 1 | 1 | 2 | 34,00 | 68,00 | Drivers queimam facilmente por picos de corrente (stall) dos motores. | A comprar |
| Módulo Regulador Buck-Boost (5V) | 1 | 1 | 2 | 25,00 | 50,00 | Essencial para não perder o projeto inteiro se houver pico na bateria. | A comprar |
| Diodo Schottky SS34 (SMD ou PTH) | 1 | 4 | 5 | 2,50 | 12,50 | Proteção reversa da bateria. Queima "para salvar" o restante do circuito. | A comprar |
| Chave DIP Switch (4 vias) | 1 | 1 | 2 | 20,00 | 40,00 | Quebra mecânica das chaves. | A comprar |
| Buzzer Ativo (3,3 V) | 1 | 1 | 2 | 10,00 | 20,00 | Componente muito barato para arriscar ficar sem diagnóstico sonoro. | A comprar |
| Transistor NPN (P2N2222A) | 1 | 4 | 5 | 2,00 | 10,00 | Acionamento do buzzer. Terminais muito frágeis. | A comprar |
| Kit Componentes Passivos (Resistores 10k, 22k, 1k, Caps 100nF e LEDs) | - | - | 1 kit | 35,00 | 35,00 | Utilizados no divisor de tensão (ADC), filtros e interface. | A comprar |
| Insumos (Estanho, Fios AWG flexíveis, Barras de Pinos e Conectores JST) | - | - | 1 kit | 40,00 | 40,00 | Consumo natural de laboratório para prototipagem e solda da PCB. | A comprar |
| **TOTAL ESTIMADO** | | | | | **R$ 529,26** | | — |


<font size="2"><p style="text-align: center">Fonte: [Ana Victória](https://github.com/navicg), [João Vitor](https://github.com/Jauzimm), [Marcus](https://github.com/MarcusVRezende), [Anderson](https://github.com/leicamAnd), [Juan](https://github.com/IndianoDev) e [Fábio](https://github.com/fabiofonteles1), 2026.</p></font>

### Orçamento do Subsistema de Software

O subsistema de software **não prevê aquisição de hardware dedicado**: o Micromouse, sensores, motores e ESP32 constam no orçamento de **Eletrônica/Hardware** e de **Estrutura**; a telemetria usa o **Bluetooth SPP integrado da ESP32** (sem módulo HM-10 adicional). A operação em **rede local** (RNF10) dispensa hospedagem em nuvem. Ferramentas de desenvolvimento, CI e banco em Docker são **open source** ou **gratuitas** (GitHub da disciplina).

| ID | Item / Serviço | Especificação / Observação | Qtd | Valor Unit. (R$) | Valor Total (R$) | Origem / Status |
| :-: | :--- | :--- | :-: | :-: | :-: | :--- |
| **1** | Notebook operacional | Receptor da ponte serial/Bluetooth, backend, front-end e demonstração na APT (equipe) | 1 | R$ 0,00 | R$ 0,00 | Já disponível (equipe) |
| **2** | Stack de desenvolvimento | Python (FastAPI), Node (React/Vite), PlatformIO, PostgreSQL via Docker, GitHub Actions | 1 | R$ 0,00 | R$ 0,00 | Open source / GitHub Education |
| **3** | Repositório e documentação | MkDocs, issues e GitHub Projects (46 tarefas de software) | 1 | R$ 0,00 | R$ 0,00 | Repositório do grupo |
| **4** | Cabo USB (firmware) | Gravação e telemetria de bancada na ESP32 (desenvolvimento) | 1 | R$ 0,00 | R$ 0,00 | Orçado em Eletrônica / equipe |
| **—** | **TOTAL ESTIMADO (SOFTWARE)** | Custos incrementais ao hardware já orçado nas outras frentes | — | — | **R$ 0,00** | — |

<font size="2"><p style="text-align: center">Fonte: [Amanda de Moura](https://github.com/AmandaaMoura), 2026. Gerência de Software — PI1 Grupo 01.</p></font>

## Orçamento Total do Projeto

| Subsistema | Valor Estimado (R$) |
|---|---:|
| Energia | R$ 157,00 |
| Eletrônica | R$ 529,26 |
| Software | R$ 0,00 |
| Estrutura | R$ 440,00 |
| **TOTAL GERAL** | **R$ 1.126,26** |