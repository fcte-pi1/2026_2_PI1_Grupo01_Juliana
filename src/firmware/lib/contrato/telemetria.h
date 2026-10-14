/*
 * Contrato de telemetria serial v1.
 *
 * Serializa as mensagens do robô no formato de
 * docs/4.4.1 - Contrato de telemetria.md: um JSON por linha, terminado em
 * "\n", com até CONTRATO_TAMANHO_MAXIMO bytes. Os exemplos de
 * src/contrato/exemplos.jsonl são a fonte única; o teste em teste/ confere
 * que estas funções os geram byte a byte.
 *
 * Não usa alocação dinâmica nem biblioteca de JSON: cada função escreve com
 * snprintf no buffer recebido.
 */
#ifndef CONTRATO_TELEMETRIA_H
#define CONTRATO_TELEMETRIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONTRATO_VERSAO 1
#define CONTRATO_TAMANHO_MAXIMO 256 /* bytes por linha, contando o "\n" */

/* Máscara de paredes do passo (referencial absoluto: N = +y, L = +x). */
#define CONTRATO_PAREDE_N 1u
#define CONTRATO_PAREDE_S 2u
#define CONTRATO_PAREDE_L 4u
#define CONTRATO_PAREDE_O 8u

typedef struct {
    uint32_t boot; /* contador de inicializações, guardado na NVS */
    uint32_t seq;  /* volta a 0 a cada boot; +1 a cada mensagem */
    uint32_t t_ms; /* milissegundos desde o boot, no instante do envio */
} contrato_cabecalho_t;

typedef enum {
    CONTRATO_ESTADO_HEALTH_CHECK,
    CONTRATO_ESTADO_RUNNING,
    CONTRATO_ESTADO_SUCCESS,
    CONTRATO_ESTADO_FAILED
} contrato_estado_t;

typedef enum {
    CONTRATO_RUMO_N,
    CONTRATO_RUMO_S,
    CONTRATO_RUMO_L,
    CONTRATO_RUMO_O
} contrato_rumo_t;

typedef enum {
    CONTRATO_EIXO_DESCONHECIDO, /* serializado como null */
    CONTRATO_EIXO_X,
    CONTRATO_EIXO_Y
} contrato_eixo_t;

typedef enum {
    CONTRATO_COMPONENTE_NENHUM, /* serializado como null */
    CONTRATO_COMPONENTE_BATERIA,
    CONTRATO_COMPONENTE_TOF_FRONTAL_ESQ,
    CONTRATO_COMPONENTE_TOF_FRONTAL_DIR,
    CONTRATO_COMPONENTE_TOF_ESQUERDO,
    CONTRATO_COMPONENTE_TOF_DIREITO,
    CONTRATO_COMPONENTE_MOTOR_ESQUERDO,
    CONTRATO_COMPONENTE_MOTOR_DIREITO,
    CONTRATO_COMPONENTE_ENCODER_ESQUERDO,
    CONTRATO_COMPONENTE_ENCODER_DIREITO
} contrato_componente_t;

typedef enum {
    CONTRATO_DIP_4X4,
    CONTRATO_DIP_8X4,
    CONTRATO_DIP_12X4,
    CONTRATO_DIP_INVALIDO
} contrato_tipo_dip_t;

typedef enum {
    CONTRATO_INICIO_NOVA,
    CONTRATO_INICIO_RETOMADA
} contrato_inicio_t;

typedef enum {
    CONTRATO_MOTIVO_NENHUM, /* serializado como null (origem web) */
    CONTRATO_MOTIVO_COLLISION,
    CONTRATO_MOTIVO_STUCK,
    CONTRATO_MOTIVO_FALHA_COMPONENTE,
    CONTRATO_MOTIVO_LOW_BATTERY,
    CONTRATO_MOTIVO_ENCERRADO_OPERADOR
} contrato_motivo_t;

typedef enum {
    CONTRATO_ORIGEM_AUTOMATICA,
    CONTRATO_ORIGEM_WEB,
    CONTRATO_ORIGEM_BOOT
} contrato_origem_t;

typedef struct {
    contrato_estado_t estado;
    uint8_t x, y;
    contrato_rumo_t rumo;
    uint16_t bat_mv;
    uint16_t vel_mm_s;
    contrato_eixo_t eixo_longo;
} contrato_tel_t;

typedef struct {
    contrato_componente_t componente;
    bool aprovado;
    bool tem_valor; /* false: valor vai como null */
    int32_t valor;  /* mV, mm ou pulsos, conforme o componente */
} contrato_hc_item_t;

typedef struct {
    bool aprovado;
    contrato_tipo_dip_t tipo_dip;
    contrato_inicio_t inicio;
} contrato_hc_resultado_t;

typedef struct {
    uint8_t x, y;
    uint8_t paredes; /* soma de CONTRATO_PAREDE_* */
} contrato_passo_t;

typedef struct {
    contrato_motivo_t motivo;
    contrato_origem_t origem;
    uint8_t x, y;
    contrato_componente_t componente;
} contrato_falha_t;

typedef struct {
    uint8_t x, y;
} contrato_sucesso_t;

/*
 * Cada função escreve a linha completa, com o "\n", em buf. Devolve o número
 * de bytes escritos (sem o terminador nulo) ou -1 se a linha não couber em
 * tam ou passar de CONTRATO_TAMANHO_MAXIMO.
 */
int contrato_tel(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_tel_t *m);
int contrato_hc_item(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_hc_item_t *m);
int contrato_hc_resultado(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_hc_resultado_t *m);
int contrato_passo(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_passo_t *m);
int contrato_falha(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_falha_t *m);
int contrato_sucesso(char *buf, size_t tam, const contrato_cabecalho_t *cab, const contrato_sucesso_t *m);

/* true se a linha recebida da ponte for o comando de interrupção da versão 1. */
bool contrato_eh_interromper(const char *linha);

#endif /* CONTRATO_TELEMETRIA_H */
