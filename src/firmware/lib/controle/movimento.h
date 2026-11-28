#ifndef MOVIMENTO_H
#define MOVIMENTO_H

/*
 * Camada de controle: transforma "avance uma célula" ou "gire 90°" em potência
 * nos motores, usando os encoders. Usa só a HAL, nunca o hardware direto.
 *
 * Primitivas (FIRM-05, #135): cada uma só inicia o movimento. Quem chama roda
 * mov_atualizar() a cada MOV_PERIODO_MS até sair de MOV_EM_CURSO, assim o mesmo
 * código anda com o relógio real e com o virtual da simulação:
 *
 *     mov_avancar_celula();
 *     while (mov_atualizar() == MOV_EM_CURSO) {
 *         esperar(MOV_PERIODO_MS);
 *     }
 *
 * O deslocamento sai da contagem de pulsos, com rampa de aceleração e
 * desaceleração. No avanço de célula, a posição lateral do eixo no corredor é
 * estimada pela odometria e pelas paredes laterais, e o robô volta para o meio;
 * a parede da frente ajusta onde o avanço termina (FIRM-10, #141). Para não
 * depender da lib sensores, quem chama repassa a leitura com mov_informar_tof().
 *
 * Roda travada (parede, roda presa) faz o movimento falhar em MOV_TRAVADO_MS.
 * Falta o FIRM-04 (#130): PID de velocidade por roda.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOV_LIVRE,     /* nenhum movimento pedido ainda, ou mov_parar() */
    MOV_EM_CURSO,
    MOV_CONCLUIDO, /* chegou nos pulsos calculados */
    MOV_FALHOU     /* roda travada, ou não chegou em MOV_TEMPO_LIMITE_MS */
} mov_estado_t;

/* Paredes e distâncias filtradas (sensores_paredes e sensores_distancia_mm); 0 mm = sem leitura. */
typedef struct {
    bool parede_frente;
    bool parede_esquerda;
    bool parede_direita;
    uint16_t frente_mm;
    uint16_t esquerda_mm;
    uint16_t direita_mm;
} mov_tof_t;

/* Por que o último movimento terminou (diagnóstico). */
typedef enum {
    MOV_FIM_NENHUM,
    MOV_FIM_ASSENTOU,    /* chegou no alvo, dentro da tolerância fina */
    MOV_FIM_PERTO,       /* parou a poucos mm do alvo (zona morta); o resto vai para o próximo */
    MOV_FIM_PAREDE,      /* encostou na parede no fim do avanço de célula */
    MOV_FIM_FREIO_FIM,   /* parada de emergência perto do fim: conta como chegada */
    MOV_FIM_TEMPO_PERTO, /* tempo esgotado perto do alvo: conta como chegada */
    MOV_FIM_TEMPO,       /* tempo esgotado longe do alvo: falha */
    MOV_FIM_TRAVOU,      /* roda travada com potência alta: falha */
    MOV_FIM_FREIO        /* parada de emergência longe do fim: falha */
} mov_fim_t;

/*
 * Resumo de um movimento terminado, para o diagnóstico na pista (linhas "#" que a ponte grava no log e não
 * manda ao backend). Pulsos com o sinal da roda andando para a frente.
 */
typedef struct {
    char tipo;               /* 'C' avanço de célula, 'A' avanço, 'R' ré, 'G' giro, 'O' arco */
    float pedido;            /* mm (avanço, ré) ou graus (giro, arco) */
    mov_fim_t fim;
    uint32_t duracao_ms;
    int32_t pulsos_esq, pulsos_dir; /* contados */
    int32_t alvo_esq, alvo_dir;     /* pedidos */
    float lateral_mm;        /* estimativa no fim */
    float rumo_graus;        /* estimativa no fim (positivo = torto para a esquerda) */
    float deriva_graus_m;    /* deriva aprendida, graus por metro */
    mov_tof_t tof;           /* última leitura dos ToF */
} mov_diag_t;

/* true uma vez por movimento terminado (concluído ou falho), com o resumo dele em *d. */
bool mov_diagnostico(mov_diag_t *d);

/* Nome curto do motivo do fim, para o log. */
const char *mov_fim_nome(mov_fim_t fim);

/* Inicia motores e encoders, com a ponte H em standby. */
void mov_iniciar(void);

/* Avança CELULA_MM: o mesmo que mov_avancar_mm(CELULA_MM, true). */
bool mov_avancar_celula(void);

/*
 * Anda mm em linha reta; negativo = ré. Com celula (só para a frente), o avanço centraliza no corredor,
 * corrige o fim pela parede da frente, compensa o erro do avanço de célula anterior e faz a parada de
 * emergência. false se já houver um movimento em curso ou mm for 0.
 */
bool mov_avancar_mm(float mm, bool celula);

/*
 * A próxima ré (mov_avancar_mm negativo) mede o rumo pela parede do lado: só serve com o robô andando ao
 * longo do corredor, e quem chama é que sabe disso. Vale para uma ré só.
 */
void mov_medir_rumo_na_re(void);

/*
 * Gira no lugar: 90 para a esquerda, -90 para a direita ou 180.
 * false para outro ângulo ou se já houver um movimento em curso.
 */
bool mov_girar(int graus);

/* Gira no lugar qualquer ângulo de -360 a 360 (positivo = esquerda), exceto 0. */
bool mov_girar_graus(float graus);

/*
 * Curva em arco: o eixo anda um arco de raio raio_mm e o robô gira graus (positivo = esquerda, até 180),
 * com a roda de dentro mais devagar. raio_mm precisa ser maior que meia bitola.
 */
bool mov_arco(float graus, float raio_mm);

/* O arco de `graus` de mov_arco, desfeito: o robô volta de ré pelo mesmo círculo (para tentar a curva de novo). */
bool mov_arco_re(float graus, float raio_mm);

/* O que o último movimento andou de fato, pelos encoders: avanço do eixo (mm) e giro (graus, + = esquerda). */
void mov_ultimo_percurso(float *mm, float *graus);

/* Cancela o movimento, corta a potência e põe a ponte H em standby. */
void mov_parar(void);

/* Um ciclo do controle, a cada MOV_PERIODO_MS. Devolve o estado depois do ciclo. */
mov_estado_t mov_atualizar(void);

mov_estado_t mov_estado(void);

/* Quanto o robô já andou no avanço em curso, em mm (média das duas rodas); 0 no giro e sem movimento. */
float mov_andado_mm(void);

/* Quanto falta para o fim do avanço em curso, em mm; 0 no giro e sem movimento. */
float mov_restante_mm(void);

/* O último avanço de célula terminou encostado na parede da frente (o nariz nela). */
bool mov_parou_na_parede(void);

/*
 * Posição lateral do eixo no corredor, em mm (positivo = à esquerda do meio). O avanço de célula a atualiza;
 * a execução a define depois de cada giro, porque o eixo fora do centro do chassi desloca o robô de lado.
 */
float mov_lateral_mm(void);
void mov_definir_lateral_mm(float mm);

/*
 * Rumo estimado do robô em relação ao corredor, em rad (positivo = virado para a esquerda). O avanço de
 * célula o corrige pelas paredes; a execução o desconta no giro seguinte e o zera depois.
 */
float mov_rumo_rad(void);
void mov_definir_rumo_rad(float rad);


/*
 * Última leitura dos ToF, a cada sensores_ler(). Cada movimento novo descarta a anterior.
 * A cópia não é protegida: chamar na mesma tarefa do mov_atualizar(), ou o FIRM-09
 * passa a leitura por uma fila.
 */
void mov_informar_tof(const mov_tof_t *tof);

/* Pulsos que cada roda precisa contar para andar mm (avanço) ou girar graus (roda direita; a esquerda é o oposto). */
int32_t mov_pulsos_avanco(float mm);
int32_t mov_pulsos_giro(int graus);

#ifdef __cplusplus
}
#endif

#endif /* MOVIMENTO_H */
