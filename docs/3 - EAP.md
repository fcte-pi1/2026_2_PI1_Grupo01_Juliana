# Estrutura Analítica de Produto

| **ID** | **Componente** | **Descrição** | **Dados Técnicos** | **Comentários** |
|:------:|----------------|---------------|--------------------|-----------------|
| 1 | **Sub-sistema: Estrutura** | Conjunto mecânico, base de fixação e sistema de locomoção do robô  | Dimensões:  103x 92 x 33mm. | Agrupa chassi, suportes, atuadores e rodagem. |
| 1.1 | Chassi | Base estrutural impressa em 3D para suporte dos motores, circuitos, suporte de pilhas e sensores.  Material: PLA. | Fabricado via FDM.  Garante leveza, rigidez mecânica e pontos de fixação com furação para parafusos M2/M3.  | Funciona como base mecânica principal e suporte dos componentes.   |
| 1.2 | Suporte | Compartimento de fixação e alojamento das pilhas.  | Transmissão direta no eixo dos motores Nema 8\. | Fixado diretamente sobre a estrutura do chassi. |
| 1.3 | Carenagem | Proteção externa da eletrônica e dos motores. | Estrutura aberta. | Não cotado no orçamento (chassi aberto exposto). |
| 1.4 | Atuadores | Motores de passo para movimentação diferencial (Esquerdo e Direito). | 2x Nema 8\. | Responsáveis pela tração e precisão dos movimentos no labirinto. |
| 1.5 | Transmissão | Acoplamento de força do motor para as rodas. | Transmissão direta no eixo dos motores Nema 11\. | Sem necessidade de caixas de redução adicionais ou correias  |
| 1.6 | Rodas/Hélices | Conjunto de rodagem e ponto de apoio. | 2x Rodas de 40 mm (borracha silicone ou neoprene) \+ 1x Roda boba omnidirecional (caster).  | Garante o contato com o solo e o equilíbrio do robô. |

<img src="https://raw.githubusercontent.com/fcte-pi1/2026_2_PI1_Grupo01_Juliana/refs/heads/feature/EAP-estruturas/docs/figs/eap_estrutura.png" alt="" width="">

| 2 | **Sub-sistema: Fonte Energética** | | | |
| 2.1 | Alimentação | | | |
| 2.2 | Eletrônica de Potência | | | |
| 2.3 | Proteções | | | |
| 2.4 | Gerenciamento de Energia | | | |
| 2.5 | | | | |
| 2.6 | | | | |
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
