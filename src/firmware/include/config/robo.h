#ifndef CONFIG_ROBO_H
#define CONFIG_ROBO_H

/*
 * Constantes físicas do robô e da pista.
 *
 * PROVISÓRIO: os valores reais vêm do ARQ-09 (#111), medidos com a Estrutura,
 * a Eletrônica e a Energia. Os atuais são típicos de um N20 com encoder e
 * servem só para a HAL simulada ter números plausíveis.
 */

/* Pista (TAP: células de 18 cm, paredes de 1,2 cm) — confirmados */
#define CELULA_MM                   180
#define PAREDE_ESPESSURA_MM         12
#define LABIRINTO_MAX_CELULAS       12   /* maior lado possível (12x4) */

/* Rodas e encoder — PROVISÓRIO */
#define RODA_DIAMETRO_MM            34.0f
#define BITOLA_MM                   90.0f   /* distância entre os centros das rodas */
#define ENCODER_PULSOS_POR_VOLTA    840     /* 7 PPR x redução 1:30 x 4 (quadratura) */
#define ROBO_RAIO_MM                50.0f   /* usado na simulação para detectar colisão */
#define VELOCIDADE_MAX_MM_S         500.0f  /* velocidade da roda com potência máxima */

/*
 * Posição dos ToF no robô — PROVISÓRIO.
 * Origem no centro do eixo das rodas; x para a frente, y para a esquerda,
 * ângulo em graus (0 = frente, 90 = esquerda).
 */
#define TOF_FE_X_MM     40.0f
#define TOF_FE_Y_MM     25.0f
#define TOF_FE_ANG      0.0f
#define TOF_FD_X_MM     40.0f
#define TOF_FD_Y_MM    -25.0f
#define TOF_FD_ANG      0.0f
#define TOF_E_X_MM      20.0f
#define TOF_E_Y_MM      30.0f
#define TOF_E_ANG       90.0f
#define TOF_D_X_MM      20.0f
#define TOF_D_Y_MM     -30.0f
#define TOF_D_ANG      -90.0f
#define TOF_ALCANCE_MAX_MM  1200  /* acima disso o VL53L0X não mede com confiança */

/* Bateria — PROVISÓRIO (a Energia confirma a razão 3,0 ou 3,2) */
#define BATERIA_DIVISOR             3.0f    /* V_bateria = V_adc x divisor */
#define BATERIA_CHEIA_MV            8400    /* LiPo 2S carregada */

#endif /* CONFIG_ROBO_H */
