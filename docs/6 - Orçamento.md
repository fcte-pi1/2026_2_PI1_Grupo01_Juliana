# Orçamento

Mande o **link público** da planilha de orçamento, e exporte em Markdown no formato a seguir:

| **ID** | **Item ou Serviço** | **Tipo** | **Previsto** | **Realizado** | **Responsável** |
|:------:|---------------------|----------|-------------:|--------------:|-----------------|
| 1 | Madeira | Matéria-prima | R$ 0,50 | R$ 0,60 | Fulano |
| 2 | Ferro | Matéria-prima | R$ 0,25 | R$ 0,25 | Fulano |
| 3 | Plástico ABS | Matéria-prima | R$ 0,25 | R$ 0,15 | Fulano |
| 4 | Arduino | Componente eletrônico | R$ 2,00 | R$ 3,00 | Beltrano |
| 5 | Fonte DC | Componente elétrico | R$ 2,00 | R$ 2,50 | Beltrano |
| 6 | Martelo | Ferramentas | R$ 3,00 | R$ 2,75 | Fulano |
| 7 | Impressão 3D | Montagem | R$ 4,00 | R$ 2,00 | Ciclano |
| 8 | | | | | |
| 9 | | | | | |
| 10 | | | | | |
| 11 | | | | | |
| 12 | | | | | |
| 13 | | | | | |
| 14 | | | | | |
| 15 | | | | | |
| 16 | | | | | |

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
