#ifndef CONFIG_PINOS_H
#define CONFIG_PINOS_H

/*
 * Mapa de pinos da ESP32 DevKit V1 (30 pinos) — arquitetura V2 (N20 + ToF).
 *
 * PROVISÓRIO: a tabela definitiva sai do ARQ-09 (#111), junto com a Eletrônica.
 * Fonte dos valores atuais: docs/4.3 (Tabela 2 e "Restrições dos pinos utilizados").
 * Só I2C_SDA/I2C_SCL e VBAT_SENSE estão confirmados no esquemático; o resto
 * foi distribuído entre os GPIOs livres e precisa ser conferido.
 *
 * -1 significa "sem pino atribuído".
 */

/* Barramento I²C dos sensores ToF (confirmado na folha de Controle) */
#define PINO_I2C_SDA        21
#define PINO_I2C_SCL        22

/* XSHUT: liga um ToF por vez na inicialização para trocar o endereço I²C */
#define PINO_XSHUT_FE       18  /* frontal esquerdo */
#define PINO_XSHUT_FD       19  /* frontal direito */
#define PINO_XSHUT_E        23  /* lateral esquerdo */
#define PINO_XSHUT_D        5   /* lateral direito (GPIO5 é strapping: nível alto no boot) */

/* Ponte H dupla (TB6612FNG): PWM de velocidade + 2 linhas de sentido por motor */
#define PINO_MOT_E_PWM      25
#define PINO_MOT_E_IN1      26
#define PINO_MOT_E_IN2      27
#define PINO_MOT_D_PWM      14
#define PINO_MOT_D_IN1      33
#define PINO_MOT_D_IN2      32
#define PINO_MOT_STBY       13  /* nível baixo = motores desligados (estado seguro) */

/* Encoders A/B (GPIO34, 35 e 39 são só entrada e não têm pull-up interno) */
#define PINO_ENC_E_A        34
#define PINO_ENC_E_B        35
#define PINO_ENC_D_A        39
#define PINO_ENC_D_B        4

/* Bateria: divisor de tensão + filtro no ADC1_CH0 (confirmado) */
#define PINO_VBAT_SENSE     36

/* Botão BOOT da placa */
#define PINO_BOOT           0

/* Sinalização */
#define PINO_LED_STATUS     2   /* LED azul da própria placa */
#define PINO_BUZZER         12  /* GPIO12 é strapping: precisa ficar em nível baixo no boot */

/* DIP switch (DIP_2 e DIP_3 em TX2/RX2, conforme a folha de Controle) */
#define PINO_DIP_1          15
#define PINO_DIP_2          17
#define PINO_DIP_3          16
#define PINO_DIP_4          (-1) /* sem GPIO livre: ver nota no README */

#endif /* CONFIG_PINOS_H */
