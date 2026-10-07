#include "tof_enderecos.h"

#include "config/pinos.h"

static const int8_t PINOS_XSHUT[TOF_QTD] = {
    PINO_XSHUT_FE,
    PINO_XSHUT_FD,
    PINO_XSHUT_E,
    PINO_XSHUT_D,
};

int8_t tof_pino_xshut(tof_id_t id)
{
    return (id >= 0 && id < TOF_QTD) ? PINOS_XSHUT[id] : -1;
}

uint8_t tof_endereco(tof_id_t id)
{
    return (uint8_t)(TOF_ENDERECO_BASE + id);
}

static tof_passo_t passo(tof_passo_tipo_t tipo, int sensor, uint8_t valor)
{
    tof_passo_t p;
    p.tipo = tipo;
    p.sensor = (int8_t)sensor;
    p.pino = sensor >= 0 ? tof_pino_xshut((tof_id_t)sensor) : -1;
    p.valor = valor;
    return p;
}

int tof_sequencia_inicio(tof_passo_t passos[TOF_PASSOS_INICIO])
{
    int n = 0;

    /* Todos desligados: ninguém responde no 0x29. */
    for (int i = 0; i < TOF_QTD; i++) {
        passos[n++] = passo(TOF_PASSO_XSHUT_BAIXO, i, 0);
    }
    passos[n++] = passo(TOF_PASSO_ESPERAR_MS, -1, TOF_ESPERA_XSHUT_MS);

    /* Um por vez: liga, espera acordar e muda o endereço. */
    for (int i = 0; i < TOF_QTD; i++) {
        passos[n++] = passo(TOF_PASSO_XSHUT_ALTO, i, 0);
        passos[n++] = passo(TOF_PASSO_ESPERAR_MS, -1, TOF_ESPERA_XSHUT_MS);
        passos[n++] = passo(TOF_PASSO_TROCAR_ENDERECO, i, tof_endereco((tof_id_t)i));
    }
    return n;
}
