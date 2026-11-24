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
 */

/* Botão BOOT */
#define BOTAO_DEBOUNCE_MS           30      /* o contato precisa ficar estável por este tempo */
#define BOTAO_LONGO_MS              1500    /* a partir daqui o toque é longo; PROVISÓRIO, confirmar com a equipe */

/* Pista — [TAP] células de 18 cm, paredes de 1,2 cm */
#define CELULA_MM                   180
#define PAREDE_ESPESSURA_MM         12
#define LABIRINTO_MAX_CELULAS       12   /* maior lado possível (12x4) */
/* Só para testes: o firmware lê o tipo no DIP switch (lib/dip). Usar onde nav.h já foi incluído. */
#define ROBO_TIPO_LABIRINTO         NAV_TIPO_4X4

/* Rodas e tração */
#define RODA_DIAMETRO_MM            34.0f   /* [EST] roda 34 x 6,5 mm */
#define BITOLA_MM                   92.5f   /* [EST] centro a centro das rodas; calibrar na bancada */
#define ROBO_RAIO_MM                69.0f   /* [EST] metade da diagonal do chassi 92 x 103 mm (138,1 mm) */
#define VELOCIDADE_MAX_MM_S         850.0f  /* [EST] máxima efetiva do N20 de 600 RPM com a roda de 34 mm */
#define VELOCIDADE_CRUZEIRO_MM_S    400.0f  /* [EST] velocidade de operação adotada */

/*
 * Primitivas de movimento (FIRM-05, #135).
 * PROVISÓRIO: valores ajustados no mundo simulado. O PID por roda do FIRM-04 (#130)
 * substitui o controle de posição, e o resto é ajustado na pista no FIRM-10 (#141).
 */
#define MOV_PERIODO_MS              5       /* 200 Hz, o mesmo laço do PID do FIRM-04 */
#define MOV_ACELERACAO_MM_S2        2000.0f /* rampa: chega a 400 mm/s em 0,2 s e 40 mm */
#define MOV_VELOCIDADE_GIRO_MM_S    250.0f  /* velocidade de cada roda no giro */
#define MOV_ATRASO_MOTOR_S          0.05f   /* constante de tempo da roda; medir na bancada (FIRM-04) */
#define MOV_KP_POSICAO              1.5f    /* potência por pulso de erro de cada roda */
#define MOV_TOLERANCIA_PULSOS       6       /* erro aceito no fim do giro, por roda (~1°) */
#define MOV_TOLERANCIA_AVANCO_PULSOS 8      /* no avanço o atrito do motor deixa a roda oscilar: ~1 mm */
#define MOV_KI_POSICAO              8.0f    /* potência por pulso de erro e por segundo: vence o atrito parado no fim do movimento */
#define MOV_INTEGRAL_MAX            200.0f  /* limite da parcela integral, em potência (anti-windup) */
#define MOV_ASSENTAR_MS             50      /* tempo dentro da tolerância para concluir */
#define MOV_TEMPO_LIMITE_MS         3000    /* sem concluir nesse tempo, o movimento falha */
#define MAQ_HC_VALIDADE_MS          600000UL /* o health-check aprovado vale por 10 min: depois disso a largada exige um novo */
#define MOV_FREIO_MS                250     /* tempo que as rodas ficam freadas na parada de emergência, antes de soltar a ponte H */
#define MOV_TRAVADO_MS              200     /* roda sem contar pulsos com potência alta: o movimento falha */
#define MOV_TRAVADO_POTENCIA        600     /* potência a partir da qual a roda parada conta como travada */

#define MOV_KP_CENTRO               0.05f   /* pulsos de desvio entre as rodas, por mm fora do centro e por ciclo (só com as duas paredes) */
#define MOV_DESVIO_MAX_PULSOS       40.0f   /* limite do desvio entre as rodas na centralização */
#define MOV_CENTRALIZAR_UMA_PAREDE  0       /* 1 centraliza com uma parede só, pela leitura de MOV_LADO_CENTRADO_MM (não conferida na pista) */
#define MOV_LADO_CENTRADO_MM        77      /* lateral com parede e o robô centrado (ver a detecção de parede) */
#define MOV_GANHO_FRONTAL           0.0f    /* 0 a 1: quanto o fim do avanço segue a parede da frente */
#define MOV_FRENTE_SOBRA_MM         17      /* distância mínima até a parede à frente que a parada de emergência garante (1,7 cm) */
#define MOV_TOF_ATRASO_S            0.075f  /* atraso do ToF frontal até o movimento: meia amostra de 50 ms mais a mediana de 3 a 20 Hz */
#define MOV_FRENTE_ALVO_MM          (CELULA_MM / 2 - PAREDE_ESPESSURA_MM / 2 - 40) /* frontal com o robô centrado e parede à frente: 44 (40 = TOF_F_X_MM) */

/* NVS: a flash aguenta cerca de 100 mil apagamentos por setor. */
#define MEM_GRAVACOES_MAX           600     /* gravações do mapa por inicialização; um robô que oscila entre células não gasta a flash */

/*
 * Leitura da parede lateral: o lateral a 45° só lê a célula com confiança logo ao entrar nela (ver a
 * detecção de parede). O avanço guarda a leitura quando já andou isto, ou seja, ~10 mm depois da
 * fronteira da célula nova. PROVISÓRIO: calibrar na pista.
 */
#define EXEC_LEITURA_LATERAL_MM     (CELULA_MM / 2 + 10)

/* Encoder — PROVISÓRIO: [EST] diz "a confirmar no datasheet do modelo adquirido" */
#define ENCODER_PULSOS_POR_VOLTA    640     /* [EST] provisório: o robô andou 23 a 25 cm por 18 cm calculados com 840; a volta à mão deu 588 e 633 */

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
#define MOTOR_AUTOTESTE_MIN_PULSOS  50      /* PROVISÓRIO: ~6 mm de roda; ajustar com a bancada */
#define MOTOR_TRAVADO_MS            1000    /* encoder parado com PWM ativo: desliga a ponte H (stuck) */

/*
 * Posição dos ToF no robô.
 * Origem no centro do eixo das rodas; x para a frente, y para a esquerda,
 * ângulo em graus (0 = frente, 90 = esquerda).
 *
 * Ângulos: [EST] 4.1, torre de sensores com o frontal a 0° e os laterais a ±45°,
 * a 25 mm do solo.
 * Posições x/y: PROVISÓRIO, dependem do CAD da torre de sensores.
 */
#define TOF_F_X_MM      40.0f
#define TOF_F_Y_MM      0.0f
#define TOF_F_ANG       0.0f
#define TOF_E_X_MM      20.0f
#define TOF_E_Y_MM      30.0f
#define TOF_E_ANG       45.0f
#define TOF_D_X_MM      20.0f
#define TOF_D_Y_MM     -30.0f
#define TOF_D_ANG      -45.0f
/* I²C dos ToF: 50 kHz tolera os jumpers da bancada; subir na placa definitiva. */
#define TOF_I2C_HZ          50000
#define TOF_ALCANCE_MAX_MM  1200  /* acima disso o VL53L0X não mede com confiança */

/*
 * Detecção de parede pelos ToF (FIRM-03, #129).
 * PROVISÓRIO: limiares tirados da geometria acima; calibrar na pista no FIRM-10 (#141).
 *
 * Com o robô centrado na célula, o frontal centrado lê ~44 mm com parede e ~224 mm sem.
 *
 * O lateral aponta 45° para a frente: mede a parede ~74 mm À FRENTE do centro
 * do robô, não ao lado. Com parede, lê ~77 mm. Sem parede, o feixe atravessa o
 * lado aberto e bate no que houver na célula vizinha; no pior caso (vizinha com
 * parede horizontal) lê (154 - p) x 1,41 mm, em que p é quanto o robô já entrou
 * na célula. Medido no mundo simulado:
 *   p de -65 a +30 mm: sem parede >= 176 mm  -> leitura confiável;
 *   p de +35 a +85 mm: 169 a 98 mm           -> parede falsa (vizinha);
 *   p de +90 a +110 mm: 77 a 91 mm           -> parede falsa (poste do canto);
 *   p acima de +115 mm: já mede a célula seguinte.
 * Por isso a parede lateral de uma célula deve ser guardada AO ENTRAR nela
 * (p entre 0 e ~20 mm), com as amostras da mediana tiradas desde ~65 mm antes
 * da fronteira. Quem escolhe o momento é o movimento (FIRM-05) / navegação (FIRM-01).
 *
 * Histerese: vira parede abaixo de ENTRA e só deixa de ser acima de SAI.
 */
#define PAREDE_FRENTE_ENTRA_MM      120
#define PAREDE_FRENTE_SAI_MM        150
#define PAREDE_LADO_ENTRA_MM        140
#define PAREDE_LADO_SAI_MM          170

#define TOF_MEDIANA_AMOSTRAS        3     /* a 20 Hz: ~100 ms de atraso, ~40 mm a 400 mm/s */
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
