#ifndef SIM_CORRIDA_H
#define SIM_CORRIDA_H

/*
 * Corrida simulada no PC (FIRM-02, #114): põe a navegação de verdade
 * (lib/navegacao) dentro de um labirinto em texto e executa o laço completo,
 * uma célula por vez:
 *
 *     ler as paredes da célula -> nav_passo() -> girar e andar uma célula
 *
 * Não há física aqui (isso é o sim_mundo): o robô sempre anda exatamente uma
 * célula e os sensores só erram quando o ruído está ligado. O objetivo é
 * testar a decisão, não o controle dos motores.
 *
 * As células entregues ao observador já estão no referencial do robô do
 * contrato de telemetria (docs/4.4.1): largada em (0, 0), y para a abertura
 * do beco da largada e x para o lado em que o labirinto se estende.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nav.h"
#include "sim_labirinto.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_LIMITE_PASSOS_PADRAO 1000

typedef enum {
    SIM_TIPO_4X4,
    SIM_TIPO_8X4,
    SIM_TIPO_12X4,
    SIM_TIPO_INVALIDO
} sim_tipo_t;

typedef enum {
    SIM_RESULTADO_SUCESSO,       /* declarou sucesso na célula de objetivo */
    SIM_RESULTADO_SEM_CAMINHO,   /* a navegação devolveu NAV_ACAO_SEM_PASSO */
    SIM_RESULTADO_COLISAO,       /* mandou andar para uma parede */
    SIM_RESULTADO_SUCESSO_FALSO, /* declarou sucesso fora do objetivo */
    SIM_RESULTADO_LIMITE         /* passou do limite de passos sem terminar */
} sim_resultado_t;

/* Uma célula alcançada, no referencial do robô. */
typedef struct {
    uint8_t x;
    uint8_t y;
    nav_direcao_t rumo;        /* para onde o robô aponta ao chegar */
    uint8_t paredes;           /* o que o robô sabe da célula, máscara NAV_PAREDE_* */
    nav_acao_t acao;           /* decisão tomada nesta célula */
    nav_direcao_t rumo_depois; /* para onde ele aponta depois do giro da ação */
} sim_celula_t;

/* Quem decide o movimento. O padrão é a navegação (nav_passo); os testes trocam por um dublê. */
typedef nav_acao_t (*sim_decisor_t)(void *ctx, nav_leitura_t leitura);

/* Chamado uma vez por célula, depois da decisão. Serve para gerar a telemetria. */
typedef void (*sim_observador_t)(void *ctx, const sim_celula_t *celula);

typedef struct {
    uint8_t ruido_pct;         /* chance (0 a 100) de cada leitura de parede vir trocada */
    uint32_t semente;          /* semente do sorteio do ruído; mesma semente, mesma corrida */
    uint16_t limite_passos;    /* 0 = SIM_LIMITE_PASSOS_PADRAO */
    sim_decisor_t decisor;     /* NULL = nav_passo */
    void *decisor_ctx;
    sim_observador_t observador; /* opcional */
    void *observador_ctx;
} sim_config_t;

typedef struct {
    sim_resultado_t resultado;
    sim_tipo_t tipo;
    uint16_t celulas;          /* células percorridas, contando as repetidas */
    uint16_t distintas;        /* células diferentes visitadas, contando a largada */
    uint16_t giros_90;
    uint16_t giros_180;
    uint16_t leituras_erradas; /* leituras trocadas pelo ruído */
    uint8_t x;                 /* célula onde a corrida terminou, referencial do robô */
    uint8_t y;
} sim_relatorio_t;

/* Tipo pelo tamanho do labirinto (8x4 e 4x8 são ambos 8x4). */
sim_tipo_t sim_tipo(const sim_labirinto_t *lab);
const char *sim_tipo_nome(sim_tipo_t tipo);
const char *sim_resultado_nome(sim_resultado_t resultado);

/*
 * Confere se o labirinto é válido para a competição: tamanho 4x4, 8x4 ou 12x4,
 * borda fechada, largada num canto com saída única (beco) e objetivo, no
 * canto oposto, alcançável. Se não for, escreve o motivo em erro.
 */
bool sim_validar(const sim_labirinto_t *lab, char *erro, size_t tamanho_erro);

/* Executa a corrida num labirinto já validado. */
void sim_correr(const sim_labirinto_t *lab, const sim_config_t *config, sim_relatorio_t *relatorio);

#ifdef __cplusplus
}
#endif

#endif /* SIM_CORRIDA_H */
