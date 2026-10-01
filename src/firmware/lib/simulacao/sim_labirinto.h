#ifndef SIM_LABIRINTO_H
#define SIM_LABIRINTO_H

/*
 * Labirinto "de verdade" usado pela simulação, lido de um texto como este (4x2):
 *
 *     +---+---+---+---+
 *     |       |       |
 *     +   +   +---+   +
 *     |   |           |
 *     +---+---+---+---+
 *
 * Cada célula ocupa 4 colunas. "---" é parede horizontal e "|" é parede
 * vertical; espaço é passagem. A célula (0, 0) é a do canto inferior esquerdo,
 * x cresce para a direita (leste) e y para cima (norte), como no docs/4.4.
 * Linhas vazias ou que começam com '#' são ignoradas (servem de comentário).
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_LAB_MAX 16

#define SIM_PAREDE_N 0x01
#define SIM_PAREDE_S 0x02
#define SIM_PAREDE_L 0x04
#define SIM_PAREDE_O 0x08

typedef struct {
    uint8_t largura;                          /* células no eixo x */
    uint8_t altura;                           /* células no eixo y */
    uint8_t paredes[SIM_LAB_MAX][SIM_LAB_MAX]; /* [x][y], máscara SIM_PAREDE_* */
} sim_labirinto_t;

/* Lê o texto. Retorna false se o desenho estiver malformado. */
bool sim_labirinto_carregar(sim_labirinto_t *lab, const char *texto);

/* true se houver parede no lado indicado; fora do labirinto é sempre parede. */
bool sim_labirinto_tem_parede(const sim_labirinto_t *lab, int x, int y, uint8_t lado);

#ifdef __cplusplus
}
#endif

#endif /* SIM_LABIRINTO_H */
