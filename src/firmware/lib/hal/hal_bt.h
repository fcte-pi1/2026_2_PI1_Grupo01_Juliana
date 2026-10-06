#ifndef HAL_BT_H
#define HAL_BT_H

/* Enlace serial com a ponte do notebook (BluetoothSerial/SPP) — FIRM-07 (#123) */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool hal_bt_iniciar(const char *nome_dispositivo);
bool hal_bt_conectado(void);

/* Envia os bytes; retorna quantos foram aceitos. */
size_t hal_bt_escrever(const char *dados, size_t tamanho);

/* Próximo byte recebido, ou -1 se não chegou nada. Nunca bloqueia. */
int hal_bt_ler_byte(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BT_H */
