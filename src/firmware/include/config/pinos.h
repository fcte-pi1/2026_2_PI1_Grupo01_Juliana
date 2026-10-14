#ifndef CONFIG_PINOS_H
#define CONFIG_PINOS_H

/*
 * Mapa de pinos da ESP32 DevKit V1 (30 pinos) — arquitetura V2 (N20 + ToF).
 * ARQ-09 (#111).
 *
 * Fonte: folha "Controle" de hw/SCH_Micromouse_2026-09-26.pdf (atualizada em
 * 28/09) e docs/4.3 ("Restrições dos pinos utilizados").
 *
 * O esquemático ainda está na transição V1 -> V2: a folha conserva as redes
 * STEP/DIR/MOT_EN do A4988. Tudo o que não é motor segue o esquemático; os
 * pinos da ponte H são PROVISÓRIOS até a Eletrônica fechar a TB6612.
 *
 * -1 significa "sem pino atribuído".
 */

/* Barramento I²C dos sensores ToF */
#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22

/* XSHUT: liga um ToF por vez na inicialização para trocar o endereço I²C */
#define PINO_XSHUT_F        33  /* frontal; PROVISÓRIO até o esquemático novo */
#define PINO_XSHUT_E        4   /* lateral esquerdo */
#define PINO_XSHUT_D        5   /* lateral direito (GPIO5 é strapping: nível alto no boot) */
/* GPIO 18 livre (era o XSHUT do segundo frontal) */

/*
 * Ponte H dupla (TB6612FNG): PWM de velocidade + 2 linhas de sentido por motor — PROVISÓRIO.
 *
 * A folha de Controle só tem as 5 redes do A4988 (STEP_E 25, DIR_E 26, STEP_D 27,
 * DIR_D 14, MOT_EN 13) e o único GPIO livre é o 15. A TB6612 pede 7 sinais, então
 * falta um pino. Uma saída é ligar PWMA/PWMB em nível alto e fazer o PWM nas
 * linhas IN, o que cabe nas 5 redes atuais. A decisão é da Eletrônica.
 *
 * Atenção: R1 (10 kΩ) puxa MOT_EN para 3,3 V. No A4988 isso desligava os
 * motores no boot; no STBY da TB6612 os liga. Precisa virar pull-down.
 */
#define PINO_MOT_E_PWM      25   /* STEP_E */
#define PINO_MOT_E_IN1      26   /* DIR_E */
#define PINO_MOT_E_IN2      15   /* único GPIO livre (strapping: nível alto no boot) */
#define PINO_MOT_D_PWM      27   /* STEP_D */
#define PINO_MOT_D_IN1      14   /* DIR_D */
#define PINO_MOT_D_IN2      (-1) /* PENDENTE: sem GPIO livre */
#define PINO_MOT_STBY       13   /* MOT_EN; nível baixo = motores desligados (estado seguro) */

/* Encoders A/B (GPIO34, 35 e 39 são só entrada e não têm pull-up interno) */
#define PINO_ENC_E_A        34
#define PINO_ENC_E_B        35
#define PINO_ENC_D_A        39
#define PINO_ENC_D_B        32

/* Bateria: divisor de tensão + filtro no ADC1_CH0 */
#define PINO_VBAT_SENSE     36

/* Botão BOOT da placa: parar corrida / apagar memória (RF41) */
#define PINO_BOOT           0

/* Sinalização */
#define PINO_LED_STATUS     2   /* LED azul da própria placa */
#define PINO_BUZZER         12  /* GPIO12 é strapping: precisa ficar em nível baixo no boot */

/* DIP switch (DIP_2 e DIP_3 em TX2/RX2) */
#define PINO_DIP_1          23
#define PINO_DIP_2          17
#define PINO_DIP_3          16
#define PINO_DIP_4          19

#endif /* CONFIG_PINOS_H */
