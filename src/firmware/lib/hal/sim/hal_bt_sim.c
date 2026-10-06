#ifdef HAL_SIMULADA

#include <string.h>

#include "hal_bt.h"
#include "hal_sim.h"

#define TAMANHO_ENTRADA 128

static hal_bt_sim_saida_t saida;

/* Fila circular com os bytes "recebidos" da ponte. */
static char entrada[TAMANHO_ENTRADA];
static size_t inicio, quantidade;

bool hal_bt_iniciar(const char *nome_dispositivo)
{
    (void)nome_dispositivo;
    inicio = quantidade = 0;
    return true;
}

bool hal_bt_conectado(void) { return saida != NULL; }

size_t hal_bt_escrever(const char *dados, size_t tamanho)
{
    if (saida == NULL) {
        return 0;
    }
    saida(dados, tamanho);
    return tamanho;
}

int hal_bt_ler_byte(void)
{
    if (quantidade == 0) {
        return -1;
    }
    char c = entrada[inicio];
    inicio = (inicio + 1) % TAMANHO_ENTRADA;
    quantidade--;
    return (unsigned char)c;
}

void hal_bt_sim_definir_saida(hal_bt_sim_saida_t nova_saida) { saida = nova_saida; }

void hal_bt_sim_injetar(const char *dados)
{
    size_t n = strlen(dados);
    for (size_t i = 0; i < n && quantidade < TAMANHO_ENTRADA; i++) {
        entrada[(inicio + quantidade) % TAMANHO_ENTRADA] = dados[i];
        quantidade++;
    }
}

#endif /* HAL_SIMULADA */
