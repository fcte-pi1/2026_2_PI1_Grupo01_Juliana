#ifndef HAL_NVS_H
#define HAL_NVS_H

/* Memória não volátil (flash/NVS) — FIRM-08 (#137) */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool hal_nvs_iniciar(void);

/* Grava um bloco de bytes com um nome (até 15 caracteres, limite da NVS). */
bool hal_nvs_gravar(const char *chave, const void *dados, size_t tamanho);

/* Lê o bloco gravado; false se a chave não existe ou o tamanho é diferente. */
bool hal_nvs_ler(const char *chave, void *dados, size_t tamanho);

bool hal_nvs_apagar(const char *chave);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NVS_H */
