#ifndef MOVIMENTO_H
#define MOVIMENTO_H

/*
 * Camada de controle: transforma "avance uma célula" ou "gire 90°" em potência
 * nos motores, usando os encoders. Usa só a HAL, nunca o hardware direto.
 *
 * Primitivas (FIRM-05, #135): cada uma só inicia o movimento. Quem chama roda
 * mov_atualizar() a cada MOV_PERIODO_MS até sair de MOV_EM_CURSO, assim o mesmo
 * código anda com o relógio real e com o virtual da simulação:
 *
 *     mov_avancar_celula();
 *     while (mov_atualizar() == MOV_EM_CURSO) {
 *         esperar(MOV_PERIODO_MS);
 *     }
 *
 * O deslocamento sai da contagem de pulsos, com rampa de aceleração e
 * desaceleração. A centralização pelos ToF laterais e a correção pela parede
 * da frente já estão no laço, com ganho 0 até o FIRM-10 (#141). Para não
 * depender da lib sensores, quem chama repassa a leitura com mov_informar_tof().
 *
 * Falta o FIRM-04 (#130): PID de velocidade por roda e detecção de travamento.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOV_LIVRE,     /* nenhum movimento pedido ainda, ou mov_parar() */
    MOV_EM_CURSO,
    MOV_CONCLUIDO, /* chegou nos pulsos calculados */
    MOV_FALHOU     /* não chegou em MOV_TEMPO_LIMITE_MS (parede, roda presa) */
} mov_estado_t;

/* Paredes e distâncias filtradas (sensores_paredes e sensores_distancia_mm); 0 mm = sem leitura. */
typedef struct {
    bool parede_frente;
    bool parede_esquerda;
    bool parede_direita;
    uint16_t frente_mm;
    uint16_t esquerda_mm;
    uint16_t direita_mm;
} mov_tof_t;

/* Inicia motores e encoders, com a ponte H em standby. */
void mov_iniciar(void);

/* Avança CELULA_MM em linha reta. false se já houver um movimento em curso. */
bool mov_avancar_celula(void);

/*
 * Gira no lugar: 90 para a esquerda, -90 para a direita ou 180.
 * false para outro ângulo ou se já houver um movimento em curso.
 */
bool mov_girar(int graus);

/* Cancela o movimento e corta a potência dos dois motores. */
void mov_parar(void);

/* Um ciclo do controle, a cada MOV_PERIODO_MS. Devolve o estado depois do ciclo. */
mov_estado_t mov_atualizar(void);

mov_estado_t mov_estado(void);

/* Última leitura dos ToF, a cada sensores_ler(). Cada movimento novo descarta a anterior. */
void mov_informar_tof(const mov_tof_t *tof);

/* Pulsos que cada roda precisa contar para andar mm (avanço) ou girar graus (roda direita; a esquerda é o oposto). */
int32_t mov_pulsos_avanco(float mm);
int32_t mov_pulsos_giro(int graus);

#ifdef __cplusplus
}
#endif

#endif /* MOVIMENTO_H */
