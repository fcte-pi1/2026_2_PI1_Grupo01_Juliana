#include "execucao.h"

#include <math.h>

#include "config/robo.h"

/*
 * Cada ação vira uma sequência de etapas (avanço, ré ou giro), calculada pela geometria do robô montado
 * (docs/decisoes/024). Referências: com o chassi centrado na célula, o eixo fica EIXO_RECUO_MM atrás do
 * centro. A posição do eixo é escrita como (longitudinal, lateral) no referencial do rumo atual, em mm a
 * partir do centro da célula; a lateral é positiva à esquerda.
 */

#define ETAPAS_MAX 8
#define GRAUS_PARA_RAD(g) ((g) * PI_F / 180.0f)

typedef enum { ETAPA_AVANCO, ETAPA_GIRO, ETAPA_ARCO, ETAPA_ARCO_RE, ETAPA_GIRO_EXATO } etapa_tipo_t;

typedef struct {
    etapa_tipo_t tipo;
    float valor;    /* mm (negativo = ré) ou graus (positivo = esquerda) no giro e no arco */
    bool celula;    /* avanço que termina na célula nova: centraliza pelas paredes */
    float lateral;  /* posição lateral do eixo no início da etapa (só no avanço de célula) */
} etapa_t;

static struct {
    exec_estado_t estado;
    etapa_t etapas[ETAPAS_MAX];
    int quantidade;
    int atual;
    /*
     * Onde o eixo parou na célula, longitudinal (mm a partir do centro): -EIXO_RECUO_MM no repouso normal, ou
     * mais à frente se o avanço terminou encostado na parede. A ação seguinte parte daí.
     */
    float longitudinal;
    bool retentou;      /* esta ação já desfez um movimento que enroscou e tentou de novo */
    bool frente_aberta; /* sem parede à frente na célula de onde a ação parte */
    uint16_t frente_mm; /* leitura do sensor da frente nessa célula (0 = não informada) */
    bool esq_aberta;    /* sem parede à esquerda nessa célula */
    bool dir_aberta;    /* sem parede à direita nessa célula */
    bool ultima_foi_re; /* a ação anterior foi uma ré: o robô está de frente para a célula de onde veio */
    bool depois_de_re;  /* idem, para a ação que está sendo planejada */
    bool alinhando;     /* girando no lugar para se endireitar antes do arco da etapa atual */
} exec;

/* Numa etapa de ré (que não usa a lateral), este valor no campo pede a medida do rumo pelas paredes. */
#define RE_MEDE_RUMO 1.0f

/* Raio de um canto do robô a partir do eixo. */
static float raio(float x, float y)
{
    return sqrtf(x * x + y * y);
}

static void reiniciar(void)
{
    exec.estado = EXEC_OCIOSO;
    exec.quantidade = 0;
    exec.atual = 0;
    exec.alinhando = false;
}

void exec_iniciar(void)
{
    reiniciar();
    exec.longitudinal = -EIXO_RECUO_MM;
    exec.ultima_foi_re = false;
    mov_definir_lateral_mm(0.0f);
    mov_definir_rumo_rad(0.0f);
}

bool exec_eh_movimento(nav_acao_t acao)
{
    return acao == NAV_ACAO_FRENTE || acao == NAV_ACAO_DIREITA || acao == NAV_ACAO_ESQUERDA ||
           acao == NAV_ACAO_MEIA_VOLTA || acao == NAV_ACAO_RE;
}

static void adicionar(etapa_tipo_t tipo, float valor, bool celula, float lateral)
{
    if (exec.quantidade < ETAPAS_MAX) {
        exec.etapas[exec.quantidade++] = (etapa_t){tipo, valor, celula, lateral};
    }
}

/* Avanço até o repouso da célula seguinte, partindo do eixo em (longitudinal, lateral). */
static void avancar_ate_a_celula(float longitudinal, float lateral)
{
    adicionar(ETAPA_AVANCO, (float)CELULA_MM - EIXO_RECUO_MM - longitudinal, true, lateral);
}

/*
 * Giro de 90° (sinal +1 à esquerda, -1 à direita) em arco: girar no lugar em volta do eixo, que fica atrás
 * do centro, passaria a quina a ~7 mm da parede da frente e do poste do lado aberto, e deixaria o robô
 * ~22 mm fora do meio. O robô recua até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro da célula e faz um
 * quarto de círculo desse raio: o centro do arco fica sobre a lateral, e o eixo termina no meio do corredor
 * novo, na fronteira da célula seguinte. A lateral de antes vira erro longitudinal, que o avanço absorve.
 */
static void planejar_giro_90(int sinal, float lateral, float longitudinal)
{
    /*
     * Sem parede à frente, o giro no lugar cabe com o eixo perto do centro da célula: as quinas da frente (a ~103 mm do
     * eixo) passam pela frente aberta e pelo lado aberto para onde o robô vai, a ~16 mm dos postes, e as de trás
     * (~56 mm) nunca chegam à parede. Não depende de onde o arco começa, que na pista (26/11/2026) errava por
     * mais do que os 12 a 20 mm de folga do arco: 11 de 47 arcos travaram. Com parede à frente a quina bateria
     * nela, e continua o arco. Depois de uma ré vale sempre: a frente é a célula de onde o robô veio, e atrás
     * pode haver parede, onde a ré do arco bateria. O eixo fica GIRO_CENTRO_ATRAS_MM atrás do centro: a quina
     * passa mais longe do poste da frente (~14 -> ~19 mm), e as de trás continuam longe.
     */
    if ((GIRO_NO_CENTRO_SE_ABERTO && exec.frente_aberta) || exec.depois_de_re) {
        float ate_o_ponto = -(longitudinal + GIRO_CENTRO_ATRAS_MM);
        if (fabsf(ate_o_ponto) >= 1.0f) {
            adicionar(ETAPA_AVANCO, ate_o_ponto, false, 0.0f);
        }
        adicionar(ETAPA_GIRO, 90.0f * (float)sinal, false, 0.0f);
        /* A lateral de antes vira longitudinal (à frente se era para o lado do giro); o recuo vira lateral. */
        avancar_ate_a_celula((float)sinal * lateral, (float)sinal * GIRO_CENTRO_ATRAS_MM);
        return;
    }
    /*
     * Recua do ponto em que parou até o eixo ficar GIRO_RAIO_ARCO_MM atrás do centro, mais GIRO_RECUO_EXTRA_MM:
     * na pista o arco sai mais aberto do que a conta, e sem a ré a mais a quina de fora pega na parede da frente.
     */
    float extra = exec.frente_aberta ? GIRO_RECUO_EXTRA_ABERTO_MM : GIRO_RECUO_EXTRA_MM;
    float recuo = GIRO_RAIO_ARCO_MM + extra + longitudinal;
    if (GIRO_FRENTE_ANTES_DO_ARCO_MM > 0 && !exec.frente_aberta && exec.frente_mm >= 20 && exec.frente_mm <= 120) {
        /*
         * Com a parede da frente à vista, a ré vai até o sensor ler GIRO_FRENTE_ANTES_DO_ARCO_MM, em vez de um
         * valor fixo a partir de onde o robô acha que parou: na pista (26/11/2026) os arcos que começaram com
         * menos de ~140 mm de frente enroscaram, e os que começaram com 155 ou mais saíram limpos.
         */
        recuo = fminf(fmaxf((float)GIRO_FRENTE_ANTES_DO_ARCO_MM - (float)exec.frente_mm, 60.0f), 135.0f);
    }
    if (recuo >= 1.0f) {
        adicionar(ETAPA_AVANCO, -recuo, false, RE_MEDE_RUMO);
    }
    adicionar(ETAPA_ARCO, 90.0f * (float)sinal, false, 0.0f);
    /* Fim do arco: longitudinal' = R + lateral (à esquerda) ou R - lateral (à direita); lateral' = 0. */
    avancar_ate_a_celula(GIRO_RAIO_ARCO_MM + (float)sinal * lateral, 0.0f);
}

/*
 * Meia-volta: o giro de 180° no lugar não cabe num corredor (as quinas da frente varrem ~100 mm para os
 * dois lados, e do meio à parede há 82,5). Ele cabe com o eixo deslocado para o lado contrário ao das
 * quinas da frente quando elas passam pela lateral: entre (raio_frente - L) e (L - raio_tras) do meio,
 * para o lado de fora do giro. A manobra: gira GIRO_ALFA, anda (de ré, quase sempre) o que leva o eixo a
 * essa faixa, e completa os 180°. Gira para o lado em que o eixo já está, para andar menos.
 */
static void planejar_meia_volta(float lateral, float longitudinal_inicial)
{
    /* Encostado na parede, a quina não gira: primeiro volta ao repouso normal. */
    if (longitudinal_inicial > -EIXO_RECUO_MM + 1.0f) {
        adicionar(ETAPA_AVANCO, -(longitudinal_inicial + EIXO_RECUO_MM), false, 0.0f);
    }
    float raio_frente = raio(EIXO_ATE_FRENTE_MM, fmaxf(MEIA_LARGURA_ESQ_MM, MEIA_LARGURA_DIR_MM));
    float raio_tras = raio(EIXO_ATE_TRAS_MM, fmaxf(MEIA_LARGURA_ESQ_MM, MEIA_LARGURA_DIR_MM));
    float deslocado = (raio_frente - raio_tras) / 2.0f; /* meio da faixa: ~22 mm */
    /* Eixo à esquerda (lateral >= 0): gira para a direita, com o eixo à esquerda; e vice-versa. */
    float sinal = lateral >= 0.0f ? -1.0f : 1.0f;
    float alvo = -sinal * deslocado;
    float alfa = GIRO_ALFA_MEIA_VOLTA_GRAUS * sinal;
    /* Andando t no rumo alfa, a lateral muda t x sen(alfa). */
    float t = (alvo - lateral) / sinf(GRAUS_PARA_RAD(alfa));
    /* Para a frente, só um pouco: pode haver parede à frente (beco). */
    t = fminf(t, 10.0f);
    float lateral_girando = lateral + t * sinf(GRAUS_PARA_RAD(alfa));
    float longitudinal = -EIXO_RECUO_MM + t * cosf(GRAUS_PARA_RAD(alfa));

    adicionar(ETAPA_GIRO, alfa, false, 0.0f);
    if (fabsf(t) >= 1.0f) {
        adicionar(ETAPA_AVANCO, t, false, 0.0f);
    }
    adicionar(ETAPA_GIRO, sinal * 180.0f - alfa, false, 0.0f);
    /* Rumo invertido: longitudinal' = -longitudinal, lateral' = -lateral. */
    longitudinal = -longitudinal;
    lateral = -lateral_girando;

    /*
     * O eixo ficou ~22 mm fora do meio. Volta para o meio com um "S": gira GIRO_ALFA para o meio, anda na
     * diagonal e desgira. A diagonal para antes de o nariz chegar perto da parede da célula seguinte (que
     * pode existir): o que sobrar de lateral, o avanço corrige pelas paredes.
     */
    if (fabsf(lateral) >= 5.0f) {
        float volta = lateral > 0.0f ? -GIRO_ALFA_MEIA_VOLTA_GRAUS : GIRO_ALFA_MEIA_VOLTA_GRAUS;
        float diagonal = -lateral / sinf(GRAUS_PARA_RAD(volta));
        float longitudinal_max = (float)CELULA_MM + CELULA_LIVRE_MEIA_MM - EIXO_ATE_FRENTE_MM - GIRO_FOLGA_FRENTE_MM;
        float diagonal_max = (longitudinal_max - longitudinal) / cosf(GRAUS_PARA_RAD(volta));
        diagonal = fminf(diagonal, diagonal_max);
        if (diagonal >= 1.0f) {
            adicionar(ETAPA_GIRO, volta, false, 0.0f);
            adicionar(ETAPA_AVANCO, diagonal, false, 0.0f);
            adicionar(ETAPA_GIRO, -volta, false, 0.0f);
            longitudinal += diagonal * cosf(GRAUS_PARA_RAD(volta));
            lateral += diagonal * sinf(GRAUS_PARA_RAD(volta));
        }
    }
    float resto = (float)CELULA_MM - EIXO_RECUO_MM - longitudinal;
    if (resto >= 1.0f) {
        avancar_ate_a_celula(longitudinal, lateral);
    } else if (resto <= -1.0f) {
        adicionar(ETAPA_AVANCO, resto, false, 0.0f); /* passou um pouco: ré até o repouso */
    }
}

/*
 * Meia-volta com a frente e um lado abertos: gira 180° no lugar, com o eixo no centro da célula, passando as
 * quinas da frente pelo lado aberto (sinal +1 à esquerda). Elas ficam a ~14 mm dos postes, contra os ~3 mm de
 * cada lado da manobra dentro do corredor.
 */
static void planejar_meia_volta_pelo_lado(int sinal, float lateral, float longitudinal)
{
    if (fabsf(longitudinal) >= 1.0f) {
        adicionar(ETAPA_AVANCO, -longitudinal, false, 0.0f);
    }
    adicionar(ETAPA_GIRO, 180.0f * (float)sinal, false, 0.0f);
    avancar_ate_a_celula(0.0f, -lateral);
}

static bool comecar_etapa(void)
{
    const etapa_t *e = &exec.etapas[exec.atual];
    if (e->tipo == ETAPA_ARCO_RE || e->tipo == ETAPA_GIRO_EXATO) {
        /* Desfazendo o que enroscou: o ângulo é o que os encoders contaram, sem descontar rumo. */
        bool comecou = e->tipo == ETAPA_ARCO_RE ? mov_arco_re(e->valor, GIRO_RAIO_ARCO_MM) : mov_girar_graus(e->valor);
        if (!comecou) {
            return false;
        }
        exec.estado = EXEC_GIRANDO;
        return true;
    }
    if (e->tipo == ETAPA_ARCO && !exec.alinhando) {
        /*
         * O arco só sai no lugar certo se começa alinhado com o corredor: descontar o torto no ângulo do arco
         * acerta o rumo do fim, mas desloca a chegada (18° de torto são ~37 mm, mais que a folga). Na pista 2
         * (27/11/2026) o robô chegou ~18° torto, a ré antes do arco mediu isso pela parede e o arco enroscou
         * na parede de fora. Torto, ele primeiro se endireita no lugar, com o eixo já recuado.
         */
        float torto = mov_rumo_rad() * 180.0f / PI_F;
        if (fabsf(torto) >= ARCO_ALINHAR_MIN_GRAUS && mov_girar_graus(-torto)) {
            mov_definir_rumo_rad(0.0f);
            exec.alinhando = true;
            exec.estado = EXEC_GIRANDO;
            return true;
        }
    }
    exec.alinhando = false;
    if (e->tipo == ETAPA_GIRO || e->tipo == ETAPA_ARCO) {
        /* Desconta o rumo torto que o avanço estimou: o giro termina alinhado com o corredor novo. */
        float graus = e->valor - mov_rumo_rad() * 180.0f / PI_F;
        bool comecou = e->tipo == ETAPA_GIRO ? mov_girar_graus(graus) : mov_arco(graus, GIRO_RAIO_ARCO_MM);
        if (!comecou) {
            return false;
        }
        mov_definir_rumo_rad(0.0f);
        exec.estado = EXEC_GIRANDO;
        return true;
    }
    if (e->celula) {
        mov_definir_lateral_mm(e->lateral);
    }
    if (!e->celula && e->valor < 0.0f && e->lateral == RE_MEDE_RUMO) {
        mov_medir_rumo_na_re();
    }
    if (!mov_avancar_mm(e->valor, e->celula)) {
        return false;
    }
    exec.estado = EXEC_AVANCANDO;
    return true;
}

bool exec_comecar(nav_acao_t acao)
{
    if (!exec_eh_movimento(acao) || exec.estado == EXEC_GIRANDO || exec.estado == EXEC_AVANCANDO) {
        return false;
    }
    reiniciar();
    exec.retentou = false;
    exec.depois_de_re = exec.ultima_foi_re;
    exec.ultima_foi_re = acao == NAV_ACAO_RE;
    float lateral = mov_lateral_mm();
    switch (acao) {
    case NAV_ACAO_ESQUERDA:
        planejar_giro_90(1, lateral, exec.longitudinal);
        break;
    case NAV_ACAO_DIREITA:
        planejar_giro_90(-1, lateral, exec.longitudinal);
        break;
    case NAV_ACAO_MEIA_VOLTA:
        if ((GIRO_NO_CENTRO_SE_ABERTO || exec.depois_de_re) && exec.frente_aberta &&
            (exec.esq_aberta || exec.dir_aberta)) {
            planejar_meia_volta_pelo_lado(exec.esq_aberta ? 1 : -1, lateral, exec.longitudinal);
        } else {
            planejar_meia_volta(lateral, exec.longitudinal);
        }
        break;
    case NAV_ACAO_RE:
        /*
         * De ré, em linha reta, até o repouso da célula de trás. A ré não se corrige pelas paredes: torto, o robô
         * sai ~3 mm de lado a cada grau (pista, 27/11/2026: ~15° no beco viraram ~45 mm na célula de trás).
         * Por isso desvira antes o que a estimativa do rumo acusa.
         */
        if (exec.depois_de_re == false) {
            float torto = mov_rumo_rad() * 180.0f / 3.14159265f;
            if (fabsf(torto) >= RE_ALINHAR_MIN_GRAUS) {
                adicionar(ETAPA_GIRO_EXATO, -torto, false, 0.0f);
            }
        }
        adicionar(ETAPA_AVANCO, -((float)CELULA_MM + exec.longitudinal + EIXO_RECUO_MM), false, RE_MEDE_RUMO);
        break;
    default:
        /* Em frente: do ponto em que parou até o repouso da célula seguinte (CELULA_MM no repouso normal). */
        avancar_ate_a_celula(exec.longitudinal, lateral);
        break;
    }
    if (!comecar_etapa()) {
        reiniciar();
        return false;
    }
    return true;
}

void exec_informar_frente(bool parede)
{
    exec.frente_aberta = !parede;
}

void exec_informar_frente_mm(uint16_t mm)
{
    exec.frente_mm = mm;
}

void exec_informar_lados(bool parede_esquerda, bool parede_direita)
{
    exec.esq_aberta = !parede_esquerda;
    exec.dir_aberta = !parede_direita;
}

/*
 * O movimento enroscou (tempo esgotado, roda travada). Uma vez por ação, o robô desfaz o que andou, pelos
 * encoders, e tenta a mesma etapa de novo: o arco volta de ré pelo mesmo círculo; o avanço volta de ré e desvira
 * o que entortou; o giro no lugar de 45° ou mais desvira e, se for de até 100°, recua 10 mm (mais longe do
 * poste da frente). Giro curto e ré não têm volta: falham.
 */
static bool tentar_de_novo(void)
{
    if (exec.retentou) {
        return false;
    }
    etapa_t falhou = exec.etapas[exec.atual];
    float mm, graus;
    mov_ultimo_percurso(&mm, &graus);
    etapa_t volta[2];
    int n = 0;
    if (falhou.tipo == ETAPA_ARCO) {
        float feito = falhou.valor > 0.0f ? fmaxf(graus, 0.0f) : fminf(graus, 0.0f);
        if (fabsf(feito) >= 3.0f) {
            volta[n++] = (etapa_t){ETAPA_ARCO_RE, feito, false, 0.0f};
        }
        /* Na pista, o arco enroscou quando começou perto demais da parede da frente: recomeça mais atrás. */
        volta[n++] = (etapa_t){ETAPA_AVANCO, -GIRO_RECUO_NOVA_TENTATIVA_MM, false, 0.0f};
    } else if (falhou.tipo == ETAPA_AVANCO && falhou.valor > 0.0f) {
        if (mm >= 5.0f) {
            volta[n++] = (etapa_t){ETAPA_AVANCO, -mm, false, RE_MEDE_RUMO};
        }
        if (fabsf(graus) >= 4.0f) {
            /*
             * O lado que pegou na parede segura a roda dele, e o robô gira para esse lado: o quanto entortou diz
             * onde está o obstáculo. Só desvirar levava o robô à mesma pose, e a segunda tentativa enroscava no
             * mesmo ponto (pista, 27/11/2026: dois avanços seguidos travados em 385/295 e 380/297 pulsos). A
             * volta gira um pouco além, para o lado livre.
             */
            float alem = fmaxf(-NOVA_TENTATIVA_DESVIO_MAX_GRAUS,
                               fminf(NOVA_TENTATIVA_DESVIO_MAX_GRAUS, NOVA_TENTATIVA_DESVIO_FATOR * graus));
            volta[n++] = (etapa_t){ETAPA_GIRO_EXATO, -(graus + alem), false, 0.0f};
        }
    } else if (falhou.tipo == ETAPA_GIRO && fabsf(falhou.valor) >= 45.0f) {
        if (fabsf(graus) >= 4.0f) {
            volta[n++] = (etapa_t){ETAPA_GIRO_EXATO, -graus, false, 0.0f};
        }
        if (fabsf(falhou.valor) <= 100.0f) {
            volta[n++] = (etapa_t){ETAPA_AVANCO, -10.0f, false, 0.0f};
        }
    } else {
        return false;
    }
    int restam = exec.quantidade - exec.atual;
    if (n + restam > ETAPAS_MAX) {
        return false;
    }
    etapa_t novas[ETAPAS_MAX];
    for (int i = 0; i < n; i++) {
        novas[i] = volta[i];
    }
    for (int i = 0; i < restam; i++) {
        novas[n + i] = exec.etapas[exec.atual + i];
    }
    for (int i = 0; i < n + restam; i++) {
        exec.etapas[i] = novas[i];
    }
    exec.quantidade = n + restam;
    exec.atual = 0;
    exec.retentou = true;
    mov_parar();
    return comecar_etapa();
}

bool exec_aproximar_da_parede(uint16_t frente_mm)
{
    if (exec.estado == EXEC_GIRANDO || exec.estado == EXEC_AVANCANDO) {
        return false;
    }
    if (frente_mm < AJUSTE_FRENTE_MIN_MM || frente_mm > AJUSTE_FRENTE_MAX_MM) {
        return false;
    }
    reiniciar();
    adicionar(ETAPA_AVANCO, (float)(frente_mm - AJUSTE_FRENTE_ALVO_MM), true, mov_lateral_mm());
    if (!comecar_etapa()) {
        reiniciar();
        return false;
    }
    return true;
}

exec_estado_t exec_atualizar(void)
{
    if (exec.estado != EXEC_GIRANDO && exec.estado != EXEC_AVANCANDO) {
        return exec.estado;
    }
    mov_estado_t m = mov_atualizar();
    if (exec.alinhando && (m == MOV_FALHOU || m == MOV_CONCLUIDO)) {
        /* Endireitou (ou não conseguiu): segue para o arco da mesma etapa. */
        if (!comecar_etapa()) {
            exec.estado = EXEC_FALHOU;
        }
        return exec.estado;
    }
    if (m == MOV_FALHOU) {
        if (!tentar_de_novo()) {
            exec.estado = EXEC_FALHOU;
        }
    } else if (m == MOV_CONCLUIDO) {
        bool ultima_celula = exec.etapas[exec.atual].celula;
        exec.atual++;
        if (exec.atual >= exec.quantidade) {
            exec.longitudinal = ultima_celula && mov_parou_na_parede()
                                    ? CELULA_LIVRE_MEIA_MM - EIXO_ATE_FRENTE_MM
                                    : -EIXO_RECUO_MM;
            exec.estado = EXEC_OCIOSO;
        } else if (!comecar_etapa()) {
            exec.estado = EXEC_FALHOU;
        }
    }
    return exec.estado;
}

exec_estado_t exec_estado(void)
{
    return exec.estado;
}

void exec_parar(void)
{
    mov_parar();
    reiniciar();
    exec.longitudinal = -EIXO_RECUO_MM;
    exec.ultima_foi_re = false;
    mov_definir_lateral_mm(0.0f);
    mov_definir_rumo_rad(0.0f);
}
