#ifndef SIM_MUNDO_H
#define SIM_MUNDO_H

/*
 * Mundo simulado: um robô de tração diferencial dentro de um labirinto.
 *
 * Modela, de forma simples, o que os drivers reais vão ler:
 *   - motores: a velocidade da roda segue a potência com um pequeno atraso;
 *   - encoders: pulsos proporcionais ao quanto cada roda andou;
 *   - ToF: distância até a primeira parede na direção de cada sensor;
 *   - bateria: tensão que cai com o tempo e com o uso dos motores;
 *   - colisão: se o corpo do robô encosta numa parede, as rodas travam.
 *
 * Os IDs de motor e de ToF seguem a mesma ordem de motor_id_t e tof_id_t da HAL.
 */

#include <stdbool.h>
#include <stdint.h>
#include "sim_labirinto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_TOF_FORA_DE_ALCANCE 8190 /* valor que o VL53L0X devolve quando não vê nada */

typedef struct {
    float x_mm;      /* a partir do canto inferior esquerdo do labirinto */
    float y_mm;
    float theta_rad; /* 0 = leste, pi/2 = norte */
    bool colidiu;    /* true se o último movimento foi bloqueado por parede */
} sim_pose_t;

/* Carrega o labirinto e põe o robô no centro de (0, 0), virado para o norte. */
bool sim_mundo_iniciar(const char *labirinto_texto);

/* Avança o tempo simulado (em passos internos de 1 ms). */
void sim_mundo_avancar(uint32_t dt_ms);

/* Usado pela HAL simulada */
uint32_t sim_tempo_ms(void);
void sim_motor_habilitar(bool habilitar);
void sim_motor_definir(int motor, int16_t potencia);
int32_t sim_encoder(int motor);
bool sim_tof_medir(int tof, uint16_t *distancia_mm);
uint16_t sim_bateria_adc_mv(void);
bool sim_boot_pressionado(void);
void sim_led_definir(bool aceso);
void sim_buzzer_definir(bool ligado);

/* Usado pelos testes para provocar situações */
void sim_definir_tof_ausente(int tof, bool ausente);
void sim_definir_bateria_mv(uint16_t mv);
void sim_definir_boot(bool pressionado);
void sim_definir_pose(float x_mm, float y_mm, float theta_rad);

/* Usado pelos testes para conferir o resultado */
sim_pose_t sim_pose(void);
bool sim_motores_habilitados(void);
bool sim_led_aceso(void);
bool sim_buzzer_ligado(void);
const sim_labirinto_t *sim_labirinto(void);

#ifdef __cplusplus
}
#endif

#endif /* SIM_MUNDO_H */
