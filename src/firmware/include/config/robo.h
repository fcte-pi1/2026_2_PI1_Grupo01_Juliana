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

/* Pista — [TAP] células de 18 cm, paredes de 1,2 cm */
#define CELULA_MM                   180
#define PAREDE_ESPESSURA_MM         12
#define LABIRINTO_MAX_CELULAS       12   /* maior lado possível (12x4) */

/* Rodas e tração */
#define RODA_DIAMETRO_MM            34.0f   /* [EST] roda 34 x 6,5 mm */
#define BITOLA_MM                   92.5f   /* [EST] centro a centro das rodas; calibrar na bancada */
#define ROBO_RAIO_MM                69.0f   /* [EST] metade da diagonal do chassi 92 x 103 mm (138,1 mm) */
#define VELOCIDADE_MAX_MM_S         850.0f  /* [EST] máxima efetiva do N20 de 600 RPM com a roda de 34 mm */
#define VELOCIDADE_CRUZEIRO_MM_S    400.0f  /* [EST] velocidade de operação adotada */

/* Encoder — PROVISÓRIO: [EST] diz "a confirmar no datasheet do modelo adquirido" */
#define ENCODER_PULSOS_POR_VOLTA    840     /* 7 PPR x redução 1:30 x 4 (quadratura) */

/*
 * Motores — [ENE] 4.2 v1.3, Tabelas 7 e 8.
 * O limite de PWM não é fixo: potência máxima = MOTOR_POTENCIA_MAX x 6000 / V_bateria_mV,
 * limitada a 100 %. Dá 71 % com 8,4 V, 81 % com 7,4 V e 100 % com as pilhas (≤ 6 V).
 */
#define MOTOR_TENSAO_NOMINAL_MV     6000    /* [EST] N20 de 6 V */
#define MOTOR_TRAVADO_MS            1000    /* encoder parado com PWM ativo: desliga a ponte H (stuck) */

/*
 * Posição dos ToF no robô.
 * Origem no centro do eixo das rodas; x para a frente, y para a esquerda,
 * ângulo em graus (0 = frente, 90 = esquerda).
 *
 * Ângulos: [EST] torre de sensores com 0° e ±45°, a 25 mm do solo.
 * Posições x/y: PROVISÓRIO, dependem do CAD da torre de sensores.
 */
#define TOF_FE_X_MM     40.0f
#define TOF_FE_Y_MM     25.0f
#define TOF_FE_ANG      0.0f
#define TOF_FD_X_MM     40.0f
#define TOF_FD_Y_MM    -25.0f
#define TOF_FD_ANG      0.0f
#define TOF_E_X_MM      20.0f
#define TOF_E_Y_MM      30.0f
#define TOF_E_ANG       45.0f
#define TOF_D_X_MM      20.0f
#define TOF_D_Y_MM     -30.0f
#define TOF_D_ANG      -45.0f
#define TOF_ALCANCE_MAX_MM  1200  /* acima disso o VL53L0X não mede com confiança */

/* Bateria — LiPo 2S 7,4 V 500 mAh [ENE] */
#define BATERIA_DIVISOR             3.2f    /* [HW] divisor 22 kΩ / 10 kΩ: V_bateria = V_adc x 3,2 */
#define BATERIA_CHEIA_MV            8400    /* [ENE] 4,2 V por célula */
#define BATERIA_CORTE_MV            7000    /* [ENE] corte por subtensão: 3,5 V por célula */
#define BATERIA_ALERTA_MV           7400    /* [ENE] alerta (LED + buzzer): 3,7 V por célula, acima do corte */

/* Fonte alternativa — 4 pilhas AA em série, selecionada por jumper [ENE] */
#define BATERIA_AA_ALERTA_MV        5000    /* [ENE] alerta de carga mínima das pilhas */

/*
 * Curvas de descarga: pares {mV, %}, da tensão maior para a menor; o firmware
 * interpola entre os pontos (FIRM-06, #128).
 * PROVISÓRIO: curva típica de LiPo e de pilha alcalina sob a corrente média de
 * 0,43 A [ENE]. Será trocada pela curva medida nos testes de energia (7.2).
 * Na LiPo, 0 % é o corte (7,0 V): é a carga que o robô ainda pode usar.
 */
#define BATERIA_CURVA_LIPO { \
    {8400, 100}, {8200, 90}, {8000, 75}, {7800, 55}, \
    {7600, 35},  {7400, 15}, {7200, 5},  {7000, 0} }
#define BATERIA_CURVA_AA { \
    {6400, 100}, {6000, 80}, {5600, 50}, {5200, 20}, \
    {5000, 10},  {4400, 0} }

#endif /* CONFIG_ROBO_H */
