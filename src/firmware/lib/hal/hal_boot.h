#ifndef HAL_BOOT_H
#define HAL_BOOT_H

/* Botão BOOT da placa (GPIO0) — FIRM-09 (#138) */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_boot_iniciar(void);

/* true enquanto o botão estiver apertado. */
bool hal_boot_pressionado(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_BOOT_H */
