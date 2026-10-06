#ifndef MOVIMENTO_H
#define MOVIMENTO_H

/*
 * Camada de controle: transforma "avance uma célula" ou "gire 90°" em potência
 * nos motores, usando os encoders (PID) — usa só a HAL, nunca o hardware direto.
 *
 * Próximas funções:
 *   FIRM-04 (#130): PID de velocidade por roda a 200 Hz e detecção de travamento.
 *   FIRM-05 (#135): bool mov_avancar_celula(void); bool mov_girar(int graus);
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Inicia motores e encoders, com a ponte H em standby. */
void mov_iniciar(void);

/* Corta a potência dos dois motores. */
void mov_parar(void);

#ifdef __cplusplus
}
#endif

#endif /* MOVIMENTO_H */
