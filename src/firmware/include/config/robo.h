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

/* Motores — [ENE] Tabela de proteções */
#define MOTOR_PWM_MAX_PERCENT       81      /* barramento de 7,4 V em motor de 6 V: média ≤ 6 V */
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

#endif /* CONFIG_ROBO_H */
