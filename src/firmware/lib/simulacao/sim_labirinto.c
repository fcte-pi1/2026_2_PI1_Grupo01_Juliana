#include "sim_labirinto.h"

#include <string.h>

#define MAX_LINHAS (2 * SIM_LAB_MAX + 1)

typedef struct {
    const char *inicio;
    int tamanho;
} linha_t;

/* Caractere na coluna pedida, tratando o que passou do fim da linha como espaço. */
static char caractere(const linha_t *linha, int coluna)
{
    return coluna < linha->tamanho ? linha->inicio[coluna] : ' ';
}

static int separar_linhas(const char *texto, linha_t *linhas)
{
    int qtd = 0;
    const char *p = texto;

    while (*p != '\0') {
        const char *fim = strchr(p, '\n');
        int tamanho = fim ? (int)(fim - p) : (int)strlen(p);

        if (tamanho > 0 && p[tamanho - 1] == '\r') {
            tamanho--;
        }
        if (tamanho > 0 && p[0] != '#') {
            if (qtd == MAX_LINHAS) {
                return -1;
            }
            linhas[qtd].inicio = p;
            linhas[qtd].tamanho = tamanho;
            qtd++;
        }
        if (!fim) {
            break;
        }
        p = fim + 1;
    }
    return qtd;
}

static void marcar(sim_labirinto_t *lab, int x, int y, uint8_t lado)
{
    if (x >= 0 && x < lab->largura && y >= 0 && y < lab->altura) {
        lab->paredes[x][y] |= lado;
    }
}

bool sim_labirinto_carregar(sim_labirinto_t *lab, const char *texto)
{
    linha_t linhas[MAX_LINHAS];
    int qtd = separar_linhas(texto, linhas);

    if (qtd < 3 || qtd % 2 == 0 || linhas[0].inicio[0] != '+') {
        return false;
    }

    int largura = (linhas[0].tamanho - 1) / 4;
    int altura = (qtd - 1) / 2;
    if (largura < 1 || largura > SIM_LAB_MAX || altura > SIM_LAB_MAX) {
        return false;
    }

    memset(lab, 0, sizeof(*lab));
    lab->largura = (uint8_t)largura;
    lab->altura = (uint8_t)altura;

    /* Linhas pares: paredes horizontais. A linha 2r fica acima da fileira r do texto. */
    for (int r = 0; r <= altura; r++) {
        for (int x = 0; x < largura; x++) {
            if (caractere(&linhas[2 * r], 4 * x + 2) == '-') {
                marcar(lab, x, altura - 1 - r, SIM_PAREDE_N); /* célula abaixo da linha */
                marcar(lab, x, altura - r, SIM_PAREDE_S);     /* célula acima da linha */
            }
        }
    }

    /* Linhas ímpares: paredes verticais, uma possível a cada 4 colunas. */
    for (int r = 0; r < altura; r++) {
        int y = altura - 1 - r;
        for (int i = 0; i <= largura; i++) {
            if (caractere(&linhas[2 * r + 1], 4 * i) == '|') {
                marcar(lab, i, y, SIM_PAREDE_O);
                marcar(lab, i - 1, y, SIM_PAREDE_L);
            }
        }
    }
    return true;
}

bool sim_labirinto_tem_parede(const sim_labirinto_t *lab, int x, int y, uint8_t lado)
{
    if (x < 0 || x >= lab->largura || y < 0 || y >= lab->altura) {
        return true;
    }
    return (lab->paredes[x][y] & lado) != 0;
}
