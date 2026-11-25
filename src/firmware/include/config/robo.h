#ifndef CONFIG_ROBO_H
#define CONFIG_ROBO_H

/*
 * Constantes físicas do robô e da pista (ARQ-09, #111).
 *
 * Cada valor indica a fonte. Os marcados como PROVISÓRIO ainda não têm fonte
 * nas outras áreas e precisam ser confirmados (datasheet, CAD ou bancada).
 *
 * Fontes:
 *   [TAP]  docs/1 - TAP.md
 *   [EST]  docs/4.1 - Projeto conceitual de estruturas.md
 *   [ENE]  docs/4.2 - Projeto conceitual de energia.md
 *   [HW]   docs/4.3 - Projeto conceitual de hardware.md e hw/SCH_Micromouse_2026-09-26.pdf
 *   [MED]  medido no robô montado e no labirinto da equipe em 24/11/2026 (régua; ver docs/decisoes/024)
 */

/* Botão BOOT */
#define BOTAO_DEBOUNCE_MS           30      /* o contato precisa ficar estável por este tempo */
#define BOTAO_LONGO_MS              1500    /* a partir daqui o toque é longo; PROVISÓRIO, confirmar com a equipe */

/* Pista — [TAP] células de 18 cm; [MED] paredes de 1,5 cm (o TAP diz 1,2 cm) */
#define CELULA_MM                   180
#define PAREDE_ESPESSURA_MM         15
#define CELULA_LIVRE_MEIA_MM        ((CELULA_MM - PAREDE_ESPESSURA_MM) / 2.0f) /* do centro da célula à face da parede: 82,5 */
#define LABIRINTO_MAX_CELULAS       12   /* maior lado possível (12x4) */
/* Só para testes: o firmware lê o tipo no DIP switch (lib/dip). Usar onde nav.h já foi incluído. */
#define ROBO_TIPO_LABIRINTO         NAV_TIPO_4X4

/* Rodas e tração */
#define RODA_DIAMETRO_MM            34.0f   /* [EST] roda 34 x 6,5 mm */
#define BITOLA_MM                   87.0f   /* [MED] centro a centro dos pneus; o CAD dizia 92,5 */
#define VELOCIDADE_MAX_MM_S         850.0f  /* [EST] máxima efetiva do N20 de 600 RPM com a roda de 34 mm */
#define VELOCIDADE_CRUZEIRO_MM_S    250.0f  /* baixada de 400: o ToF lê a cada 12 mm e o robô para no ponto */

/*
 * Geometria do robô montado [MED]. O CAD do docs/4.1 punha o eixo no meio de um chassi de 92 x 103 mm;
 * no robô o eixo das rodas fica perto da traseira. Origem no ponto médio entre as rodas (o centro do giro
 * no lugar), x para a frente e y para a esquerda.
 *
 * Consequências (docs/decisoes/024):
 *   - parado com o chassi centrado na célula, o eixo fica EIXO_RECUO_MM atrás do centro da célula;
 *   - a quina da frente gira num raio de ~100 mm, mais que os 82,5 mm do centro à parede: o giro de 90°
 *     precisa de espaço à frente, e o de 180° não cabe no lugar num corredor (a execução faz uma manobra).
 */
#define EIXO_ATE_FRENTE_MM          87.5f   /* do eixo das rodas à frente do robô */
#define EIXO_ATE_TRAS_MM            25.0f   /* do eixo à traseira */
#define MEIA_LARGURA_ESQ_MM         50.0f   /* do meio à lateral esquerda, contando os fios que sobram */
#define MEIA_LARGURA_DIR_MM         47.5f   /* do meio à lateral direita */
#define EIXO_RECUO_MM               ((EIXO_ATE_FRENTE_MM - EIXO_ATE_TRAS_MM) / 2.0f) /* 31,25 */

/*
 * Primitivas de movimento (FIRM-05, #135).
 * Valores ajustados no mundo simulado; os da pista são ajustados no FIRM-10 (#141).
 */
#define MOV_PERIODO_MS              5       /* 200 Hz, o mesmo laço do PID do FIRM-04 */
#define MOV_ACELERACAO_MM_S2        1000.0f /* rampa: chega a 250 mm/s em 0,25 s e 31 mm; menos patinagem */
#define MOV_VELOCIDADE_GIRO_MM_S    150.0f  /* velocidade de cada roda no giro: devagar, perto das paredes */
#define MOV_ATRASO_MOTOR_S          0.05f   /* constante de tempo da roda; medir na bancada (FIRM-04) */
#define MOV_KP_POSICAO              1.5f    /* potência por pulso de erro de cada roda */
#define MOV_TOLERANCIA_PULSOS       6       /* erro aceito no fim do giro, por roda (~1°) */
#define MOV_TOLERANCIA_AVANCO_PULSOS 8      /* no avanço o atrito do motor deixa a roda oscilar: ~1,5 mm */
#define MOV_KI_POSICAO              25.0f   /* potência por pulso de erro e por segundo: vence o atrito parado no fim do movimento */
#define MOV_INTEGRAL_MAX            500.0f  /* limite da parcela integral, em potência (anti-windup) */
/*
 * Zona morta do N20 no chão: abaixo dessa potência a roda parada não sai do lugar. Na pista (24/11/2026) o robô
 * parou a ~1 cm do fim de uma célula e empurrou até o tempo limite. Roda parada longe do alvo recebe pelo menos isto.
 */
#define MOV_ZONA_MORTA              250.0f
#define MOV_INTEGRAL_SOLTA          0.8f    /* fator por ciclo da integral com a roda andando: desgrudou, a força extra some */
/*
 * Perto basta: com a trajetória no fim e as duas rodas paradas há MOV_PERTO_PARADO_MS, um erro de até isto conta
 * como chegada. O que faltou entra no movimento seguinte (resíduo). Com zona morta alta, cada vez que a roda
 * desgruda ela anda mais que a tolerância fina, e sem isto o movimento esgotaria o tempo e falharia.
 */
#define MOV_PERTO_AVANCO_PULSOS     32      /* ~6 mm */
#define MOV_PERTO_GIRO_PULSOS       12      /* ~2 mm de roda, ~2,5° */
#define MOV_PERTO_PARADO_MS         150
/* No tempo limite, um erro de até 2,5 x o "perto" (~15 mm no avanço, ~6° no giro) ainda conta como chegada. */
#define MOV_ASSENTAR_MS             50      /* tempo dentro da tolerância para concluir */
#define MOV_TEMPO_LIMITE_MS         3000    /* sem concluir nesse tempo, o movimento falha */
#define MAQ_HC_VALIDADE_MS          600000UL /* o health-check aprovado vale por 10 min: depois disso a largada exige um novo */
#define MOV_FREIO_MS                250     /* tempo que as rodas ficam freadas na parada de emergência, antes de soltar a ponte H */
#define MOV_TRAVADO_MS              200     /* roda sem contar pulsos com potência alta: o movimento falha */
#define MOV_TRAVADO_POTENCIA        600     /* potência a partir da qual a roda parada conta como travada */

/*
 * Centralização no corredor (FIRM-10). O avanço estima a posição lateral do eixo e o rumo do robô em relação
 * ao corredor: a odometria prevê, e cada leitura das paredes laterais corrige (o quanto a lateral muda ao
 * longo do avanço mostra o rumo). O rumo pedido leva o eixo ao meio, e o robô para alinhado com o corredor.
 * A execução informa onde o eixo ficou depois de cada giro (o eixo atrás do centro desloca o robô de lado)
 * e desconta o rumo estimado no giro seguinte.
 */
#define MOV_CENTRO_GANHO_RAD_POR_MM 0.02f   /* rumo pedido por mm fora do centro: o desvio cai à metade em ~35 mm */
#define MOV_CENTRO_RUMO_MAX_RAD     0.15f   /* ~8,6°: o nariz não vai para cima da parede */
#define MOV_CENTRO_FUSAO            0.4f    /* peso de cada leitura lateral nova contra a estimativa da odometria */
#define MOV_RUMO_FUSAO              0.03f   /* quanto da diferença entre medida e estimativa, por mm, vira rumo */
/*
 * Leituras laterais que não servem para centralizar: acima de MOV_LATERAL_MAX_MM o robô estaria com o eixo a
 * mais de ~40 mm do meio (dentro da outra parede); a mediana dá valores assim quando o feixe passa por um
 * poste ou pela borda de uma abertura. E uma leitura que discorda da estimativa mais que o portão é descartada.
 */
#define MOV_LATERAL_MAX_MM          85
#define MOV_CENTRO_PORTAO_MM        20.0f
#define MOV_CENTRO_PORTAO_LEITURAS  3       /* tantas leituras seguidas fora do portão, e a estimativa é que está errada */
#define MOV_RUMO_MAX_RAD            0.3f    /* ~17°: limite da estimativa do rumo */
/*
 * Deriva: uma roda um pouco maior que a outra entorta o robô sempre para o mesmo lado, sem o encoder ver.
 * A cada avanço com paredes, o que o rumo precisou ser corrigido por mm andado vira deriva aprendida, que os
 * avanços seguintes já descontam.
 */
#define MOV_DERIVA_APRENDIZADO      0.7f
#define MOV_DERIVA_MAX_RAD_POR_MM   0.0005f /* ~2,9° a cada 100 mm: uma roda ~2,5% maior */
#define MOV_CENTRO_FIM_MM           40.0f   /* nos últimos mm o rumo pedido cai a zero: o robô para reto */
#define MOV_RUMO_PASSO_PULSOS       2.0f    /* quanto o desvio entre as rodas muda por ciclo, no máximo */

#define MOV_GANHO_FRONTAL           0.5f    /* 0 a 1: quanto o fim do avanço segue a parede da frente */
#define MOV_FRONTAL_JANELA_MM       40.0f   /* só corrige pela parede se ela estiver a isto do fim previsto */
#define MOV_FRONTAL_MIN_MM          (TOF_MIN_CONFIAVEL_MM + 10) /* folga do ruído: perto disso a leitura pode ser comprimida */
/*
 * A correção frontal só pode encurtar o avanço (e esticar no máximo isto): na pista, um frontal que lia a parede
 * mais longe do que ela estava fez o robô passar do ponto e encostar nela.
 */
#define MOV_FRONTAL_EXTENSAO_MM     5.0f
/*
 * Encostou na parede no fim do avanço de célula: a roda para de avançar, a até MOV_PAREDE_FIM_MM do fim previsto,
 * por MOV_PAREDE_PARADO_MS. Conta como chegada, com o nariz na parede (posição conhecida), em vez de empurrar até
 * o tempo limite e falhar.
 */
#define MOV_PAREDE_FIM_MM           50.0f
#define MOV_PAREDE_PARADO_MS        300
#define MOV_PAREDE_POTENCIA         450.0f  /* só é parede se as duas rodas empurram com isto e não andam (não é zona morta) */
#define MOV_FRENTE_SOBRA_MM         10      /* distância mínima até a parede à frente que a parada de emergência garante */
#define MOV_FRENTE_FIM_MM           35.0f   /* parada de emergência a isto do fim do avanço conta como chegada, não falha */
#define MOV_TOF_ATRASO_S            0.075f  /* atraso do ToF frontal até o movimento: meia amostra de 50 ms mais a mediana de 3 a 20 Hz */

/* NVS: a flash aguenta cerca de 100 mil apagamentos por setor. */
#define MEM_GRAVACOES_MAX           600     /* gravações do mapa por inicialização; um robô que oscila entre células não gasta a flash */

/* Encoder — [MED] 10 voltas marcadas, girando à mão: 5759 e 5743 (esq.), 5716 e 5752 (dir.) */
#define ENCODER_PULSOS_POR_VOLTA    574     /* com 640 o robô andava 18 a 20 cm por célula e girava ~107° por 90° */

/* Derivadas: o controle, o mundo simulado e os testes usam as mesmas. */
#define PI_F                        3.14159265f
#define MM_POR_PULSO                (PI_F * RODA_DIAMETRO_MM / ENCODER_PULSOS_POR_VOLTA)

/*
 * Motores — [ENE] 4.2 v1.3, Tabelas 7 e 8.
 * O limite de PWM não é fixo: potência máxima = MOTOR_POTENCIA_MAX x 6000 / V_bateria_mV,
 * limitada a 100 %. Dá 71 % com 8,4 V, 81 % com 7,4 V e 100 % com as pilhas (≤ 6 V).
 */
#define MOTOR_TENSAO_NOMINAL_MV     6000    /* [EST] N20 de 6 V */
/* Autoteste de motor e encoder do health-check: giro curto de uma roda por vez, rodas no ar. */
#define MOTOR_AUTOTESTE_POTENCIA    300     /* 30 %, a mesma da bancada dos drivers; ~500 pulsos esperados no tempo abaixo */
#define MOTOR_AUTOTESTE_MS          300
#define MOTOR_AUTOTESTE_MIN_PULSOS  50      /* PROVISÓRIO: ~9 mm de roda; ajustar com a bancada */
#define MOTOR_TRAVADO_MS            1000    /* encoder parado com PWM ativo: desliga a ponte H (stuck) */

/*
 * Posição dos ToF no robô montado [MED].
 * Origem no centro do eixo das rodas; x para a frente, y para a esquerda,
 * ângulo em graus (0 = frente, 90 = esquerda).
 *
 * O frontal fica no nariz; os laterais, em braços a 65 mm à frente do eixo, apontam para os lados (90°, não
 * os ±45° do CAD). Os três ficam a ~4 cm do chão, acima das paredes de 3,5 cm do labirinto da equipe, e os
 * braços dos laterais estão um pouco tortos (o direito para baixo, o esquerdo para cima).
 */
#define TOF_F_X_MM      87.5f
#define TOF_F_Y_MM      0.0f
#define TOF_F_ANG       0.0f
#define TOF_E_X_MM      65.0f
#define TOF_E_Y_MM      40.0f
#define TOF_E_ANG       90.0f
#define TOF_D_X_MM      65.0f
#define TOF_D_Y_MM     -40.0f
#define TOF_D_ANG      -90.0f
/*
 * Erro de cada ToF, descontado da leitura [MED] (régua contra a parede): o esquerdo lê ~2 cm a mais em
 * todas as distâncias; o frontal e o direito acertam a partir de ~4 cm. Abaixo de TOF_MIN_CONFIAVEL_MM os
 * três leem de 30 a 40 mm seja qual for a distância (3 cm vira 40, 2 cm vira 30): a leitura não mede.
 */
#define TOF_F_DESVIO_MM     0
#define TOF_E_DESVIO_MM     20
/*
 * Direito: na bancada (contra uma madeira) acertava, mas no labirinto, com o braço torto para baixo, leu ~20 mm a
 * mais que o esquerdo em todas as células com parede dos dois lados (24/11/2026: 61/45, 68/40, 63/45, 62/47 e
 * 70/35 mm), em qualquer rumo. Sem o desconto, o robô se achava à esquerda do meio e andava raspando a direita.
 */
#define TOF_D_DESVIO_MM     20
#define TOF_MIN_CONFIAVEL_MM 40   /* na leitura bruta, antes do desconto do erro */
/*
 * Piso do direito na leitura já descontada: a compressão é do sensor e vale para a leitura bruta, então, com os
 * 20 mm de erro, o direito mede bem até ~20 mm. Com o piso de 40 na leitura descontada (até 24/11/2026), o robô
 * centrado lia ~42 mm e bastava ficar 3 mm à direita do meio para o direito parar de medir.
 */
#define TOF_D_MIN_CONFIAVEL_MM (TOF_MIN_CONFIAVEL_MM - TOF_D_DESVIO_MM)
/*
 * O esquerdo é diferente: lê ~2 cm a mais em todas as distâncias, inclusive de perto (1 cm vira 30, 2 cm vira
 * 40), então, descontado o erro, a leitura dele mede bem até ~1,5 cm. Na pista (24/11/2026), tratá-lo como os
 * outros fez o robô achar que estava colado na parede (lateral 51 mm) quando estava a ~27 mm do meio.
 */
#define TOF_E_MIN_CONFIAVEL_MM 15
/* I²C dos ToF: 50 kHz tolera os jumpers da bancada; subir na placa definitiva. */
#define TOF_I2C_HZ          50000
#define TOF_ALCANCE_MAX_MM  1200  /* acima disso o VL53L0X não mede com confiança */

/*
 * Frontal com o chassi centrado e parede à frente: 82,5 - (87,5 - 31,25) = 26 mm, abaixo da faixa confiável
 * (o sensor mostra 30 a 40). A correção pela frente só usa leituras confiáveis.
 */
#define MOV_FRENTE_ALVO_MM          (CELULA_LIVRE_MEIA_MM - (TOF_F_X_MM - EIXO_RECUO_MM))

/*
 * Detecção de parede pelos ToF (FIRM-03, #129).
 *
 * Com o chassi centrado na célula, o frontal lê ~26 mm com parede e ~206 mm sem (a parede da célula
 * seguinte).
 *
 * Os laterais apontam para os lados, a 65 mm à frente do eixo: parado, o sensor fica 34 mm à frente do
 * centro da célula e mede a parede da própria célula. Com parede e o robô centrado, leem ~42 mm; com o robô
 * deslocado depois de um giro, de ~20 a ~65 mm. Sem parede, o feixe atravessa a célula vizinha e lê mais de
 * ~220 mm (ou nada).
 *
 * Histerese: vira parede abaixo de ENTRA e só deixa de ser acima de SAI.
 */
#define PAREDE_FRENTE_ENTRA_MM      120
#define PAREDE_FRENTE_SAI_MM        150
#define PAREDE_LADO_ENTRA_MM        120
#define PAREDE_LADO_SAI_MM          150

/*
 * Giros com o eixo atrás do centro (docs/decisoes/024). Raios do giro no lugar, a partir do eixo: quina da
 * frente ~100 mm, quina de trás ~56 mm.
 *   - 90°: curva em arco. O robô recua até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro e faz um quarto de
 *     círculo desse raio: a quina passa a ~10 mm da parede da frente, longe dos postes, e o robô termina no
 *     meio do corredor novo. Girar no lugar deixava ~7 mm de folga e o robô ~22 mm fora do meio.
 *   - 180°: manobra. Giro curto, ré que leva o eixo para o lado, e o resto do giro, que cabe no corredor com
 *     o eixo ~22 mm fora do meio (~4 mm de folga de cada lado); depois um "S" volta o robô ao meio.
 */
#define GIRO_RAIO_ARCO_MM           90.0f   /* raio do arco do eixo no giro de 90° */
#define GIRO_ALFA_MEIA_VOLTA_GRAUS  15.0f   /* giro curto da meia-volta e do "S": com o eixo no meio, a quina vai a ~71 mm do centro */
#define GIRO_FOLGA_FRENTE_MM        10.0f   /* folga do nariz até a parede da célula seguinte no "S" da meia-volta */

#define TOF_MEDIANA_AMOSTRAS        3     /* a 20 Hz: ~100 ms de atraso, ~25 mm a 250 mm/s */
#define TOF_FALHAS_PARA_DESCARTAR   3     /* leituras seguidas sem resposta (150 ms a 20 Hz) */
#define TOF_AUTOTESTE_AMOSTRAS      5
#define TOF_AUTOTESTE_MIN_MM        20    /* abaixo disso, sensor tampado ou com defeito */

/* Bateria — LiPo 2S 7,4 V 650 mAh [ENE] (4.2 v1.5) */
#define BATERIA_DIVISOR             3.2f    /* [HW] divisor 22 kΩ / 10 kΩ depois do diodo: V_ponte = V_adc x 3,2 */
#define BATERIA_QUEDA_DIODO_MV      350     /* [ENE] queda do SS34, somada à leitura antes dos limites; calibrar em 7.2 */
#define BATERIA_MEDIA_AMOSTRAS      16      /* média móvel contra o ruído das escovas (4.2 pede no mínimo 8) */
#define BATERIA_LIMIAR_FONTE_MV     6700    /* acima disso é a LiPo (nunca abaixo de 7,0 V em uso); abaixo, as pilhas (no máximo ~6,4 V) */
#define BATERIA_CHEIA_MV            8400    /* [ENE] 4,2 V por célula */
#define BATERIA_CORTE_MV            7000    /* [ENE] corte por subtensão: 3,5 V por célula */
#define BATERIA_ALERTA_MV           7400    /* [ENE] alerta (LED + buzzer): 3,7 V por célula, acima do corte */

/* Fonte alternativa — 4 pilhas AA em série, selecionada por jumper [ENE] */
#define BATERIA_AA_ALERTA_MV        5000    /* [ENE] alerta de carga mínima das pilhas */
#define BATERIA_AA_CORTE_MV         5000    /* [ENE] o 4.2 não separa corte das pilhas: low_battery abaixo de 5,0 V */

/*
 * Curvas de descarga: pares {mV, %}, da tensão maior para a menor; o firmware
 * interpola entre os pontos (FIRM-06, #128).
 * PROVISÓRIO: curva típica de LiPo e de pilha alcalina sob a corrente média de
 * 0,43 A [ENE]. Será trocada pela curva medida nos testes de energia (7.2).
 * Nas duas fontes, 0 % é o corte: é a carga que o robô ainda pode usar.
 */
#define BATERIA_CURVA_LIPO { \
    {8400, 100}, {8200, 90}, {8000, 75}, {7800, 55}, \
    {7600, 35},  {7400, 15}, {7200, 5},  {7000, 0} }
#define BATERIA_CURVA_AA { \
    {6400, 100}, {6000, 80}, {5600, 50}, {5200, 20}, \
    {5000, 0} }

#endif /* CONFIG_ROBO_H */
