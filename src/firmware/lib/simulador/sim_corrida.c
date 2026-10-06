#include "sim_corrida.h"

#include <stdio.h>
#include <string.h>

static const uint8_t LADO[4] = {SIM_PAREDE_N, SIM_PAREDE_L, SIM_PAREDE_S, SIM_PAREDE_O};
static const uint8_t MASCARA_NAV[4] = {NAV_PAREDE_N, NAV_PAREDE_L, NAV_PAREDE_S, NAV_PAREDE_O};
static const int DX[4] = {0, 1, 0, -1};
static const int DY[4] = {1, 0, -1, 0};

static nav_direcao_t girar(nav_direcao_t d, nav_acao_t acao) { return nav_direcao_apos(d, acao); }

static bool parede(const sim_labirinto_t *lab, int x, int y, nav_direcao_t d)
{
    return sim_labirinto_tem_parede(lab, x, y, LADO[d]);
}

sim_tipo_t sim_tipo(const sim_labirinto_t *lab)
{
    int menor = lab->largura < lab->altura ? lab->largura : lab->altura;
    int maior = lab->largura < lab->altura ? lab->altura : lab->largura;

    if (menor != 4) {
        return SIM_TIPO_INVALIDO;
    }
    switch (maior) {
    case 4:
        return SIM_TIPO_4X4;
    case 8:
        return SIM_TIPO_8X4;
    case 12:
        return SIM_TIPO_12X4;
    default:
        return SIM_TIPO_INVALIDO;
    }
}

const char *sim_tipo_nome(sim_tipo_t tipo)
{
    static const char *const NOMES[] = {"4x4", "8x4", "12x4", "invalido"};
    return NOMES[tipo];
}

const char *sim_resultado_nome(sim_resultado_t resultado)
{
    static const char *const NOMES[] = {"sucesso", "sem_caminho", "colisao", "sucesso_falso", "limite"};
    return NOMES[resultado];
}

/* Saída do beco da largada. Só faz sentido se a largada tiver 3 paredes. */
static nav_direcao_t abertura_da_largada(const sim_labirinto_t *lab)
{
    for (int d = 0; d < 4; d++) {
        if (!parede(lab, lab->largada_x, lab->largada_y, (nav_direcao_t)d)) {
            return (nav_direcao_t)d;
        }
    }
    return NAV_NORTE;
}

static bool objetivo_alcancavel(const sim_labirinto_t *lab, int ox, int oy)
{
    bool visitada[SIM_LAB_MAX][SIM_LAB_MAX] = {{false}};
    int fila[SIM_LAB_MAX * SIM_LAB_MAX][2];
    int inicio = 0, fim = 0;

    fila[fim][0] = lab->largada_x;
    fila[fim][1] = lab->largada_y;
    fim++;
    visitada[lab->largada_x][lab->largada_y] = true;

    while (inicio < fim) {
        int x = fila[inicio][0];
        int y = fila[inicio][1];
        inicio++;
        if (x == ox && y == oy) {
            return true;
        }
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d];
            int ny = y + DY[d];
            if (!parede(lab, x, y, (nav_direcao_t)d) && !visitada[nx][ny]) {
                visitada[nx][ny] = true;
                fila[fim][0] = nx;
                fila[fim][1] = ny;
                fim++;
            }
        }
    }
    return false;
}

bool sim_validar(const sim_labirinto_t *lab, char *erro, size_t tamanho_erro)
{
    int w = lab->largura;
    int h = lab->altura;
    int lx = lab->largada_x;
    int ly = lab->largada_y;

    if (sim_tipo(lab) == SIM_TIPO_INVALIDO) {
        snprintf(erro, tamanho_erro, "tamanho %dx%d nao e 4x4, 8x4 nem 12x4", w, h);
        return false;
    }

    /* sim_labirinto_tem_parede trata fora do labirinto como parede, então a
     * borda é conferida direto na máscara de cada célula. */
    for (int x = 0; x < w; x++) {
        if (!(lab->paredes[x][0] & SIM_PAREDE_S) || !(lab->paredes[x][h - 1] & SIM_PAREDE_N)) {
            snprintf(erro, tamanho_erro, "borda aberta na coluna %d", x);
            return false;
        }
    }
    for (int y = 0; y < h; y++) {
        if (!(lab->paredes[0][y] & SIM_PAREDE_O) || !(lab->paredes[w - 1][y] & SIM_PAREDE_L)) {
            snprintf(erro, tamanho_erro, "borda aberta na linha %d", y);
            return false;
        }
    }

    if ((lx != 0 && lx != w - 1) || (ly != 0 && ly != h - 1)) {
        snprintf(erro, tamanho_erro, "largada (%d, %d) nao fica num canto", lx, ly);
        return false;
    }

    int paredes = 0;
    for (int d = 0; d < 4; d++) {
        paredes += parede(lab, lx, ly, (nav_direcao_t)d);
    }
    if (paredes != 3) {
        snprintf(erro, tamanho_erro, "largada precisa de uma unica saida (tem %d)", 4 - paredes);
        return false;
    }

    if (!objetivo_alcancavel(lab, w - 1 - lx, h - 1 - ly)) {
        snprintf(erro, tamanho_erro, "objetivo (%d, %d) inalcancavel", w - 1 - lx, h - 1 - ly);
        return false;
    }
    return true;
}

/* ---- Referencial do robô ---------------------------------------------- */

typedef struct {
    int ox, oy;            /* largada, no referencial do labirinto */
    nav_direcao_t eixo_y;  /* direção absoluta que vira +y para o robô */
    nav_direcao_t eixo_x;  /* direção absoluta que vira +x para o robô */
} referencial_t;

static referencial_t montar_referencial(const sim_labirinto_t *lab)
{
    referencial_t r;
    r.ox = lab->largada_x;
    r.oy = lab->largada_y;
    r.eixo_y = abertura_da_largada(lab);

    /* +x é o lado em que o labirinto continua; do outro lado da largada só há borda. */
    nav_direcao_t direita = girar(r.eixo_y, NAV_ACAO_DIREITA);
    int nx = r.ox + DX[direita];
    int ny = r.oy + DY[direita];
    bool dentro = nx >= 0 && nx < lab->largura && ny >= 0 && ny < lab->altura;
    r.eixo_x = dentro ? direita : girar(r.eixo_y, NAV_ACAO_ESQUERDA);
    return r;
}

static nav_direcao_t direcao_no_robo(const referencial_t *r, nav_direcao_t d)
{
    if (d == r->eixo_y) {
        return NAV_NORTE;
    }
    if (d == girar(r->eixo_y, NAV_ACAO_MEIA_VOLTA)) {
        return NAV_SUL;
    }
    return d == r->eixo_x ? NAV_LESTE : NAV_OESTE;
}

static void celula_no_robo(const referencial_t *r, int x, int y, uint8_t *rx, uint8_t *ry)
{
    int dx = x - r->ox;
    int dy = y - r->oy;
    *rx = (uint8_t)(dx * DX[r->eixo_x] + dy * DY[r->eixo_x]);
    *ry = (uint8_t)(dx * DX[r->eixo_y] + dy * DY[r->eixo_y]);
}

/* ---- Corrida ------------------------------------------------------------ */

/* xorshift32: o mesmo sorteio em qualquer plataforma, ao contrário de rand(). */
static uint32_t sortear(uint32_t *estado)
{
    uint32_t v = *estado;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    *estado = v;
    return v;
}

static bool ler_sensor(bool verdade, uint8_t ruido_pct, uint32_t *sorteio, uint16_t *erradas)
{
    if (ruido_pct > 0 && sortear(sorteio) % 100 < ruido_pct) {
        (*erradas)++;
        return !verdade;
    }
    return verdade;
}

static nav_acao_t decidir_com_nav(void *ctx, nav_leitura_t leitura)
{
    return nav_passo((nav_estado_t *)ctx, leitura);
}

void sim_correr(const sim_labirinto_t *lab, const sim_config_t *config, sim_relatorio_t *rel)
{
    nav_estado_t nav;
    sim_decisor_t decidir = config->decisor;
    void *decisor_ctx = config->decisor_ctx;
    if (decidir == NULL) {
        nav_iniciar(&nav);
        decidir = decidir_com_nav;
        decisor_ctx = &nav;
    }

    uint16_t limite = config->limite_passos ? config->limite_passos : SIM_LIMITE_PASSOS_PADRAO;
    uint32_t sorteio = config->semente ? config->semente : 1; /* xorshift não sai do 0 */
    referencial_t ref = montar_referencial(lab);
    bool visitada[SIM_LAB_MAX][SIM_LAB_MAX] = {{false}};

    int x = ref.ox;
    int y = ref.oy;
    nav_direcao_t rumo = ref.eixo_y;
    int objetivo_x = lab->largura - 1 - ref.ox;
    int objetivo_y = lab->altura - 1 - ref.oy;

    memset(rel, 0, sizeof(*rel));
    rel->tipo = sim_tipo(lab);
    visitada[x][y] = true;
    rel->distintas = 1;

    for (;;) {
        nav_direcao_t esquerda = girar(rumo, NAV_ACAO_ESQUERDA);
        nav_direcao_t direita = girar(rumo, NAV_ACAO_DIREITA);
        nav_direcao_t tras = girar(rumo, NAV_ACAO_MEIA_VOLTA);

        nav_leitura_t leitura;
        leitura.frente = ler_sensor(parede(lab, x, y, rumo), config->ruido_pct, &sorteio, &rel->leituras_erradas);
        leitura.esquerda = ler_sensor(parede(lab, x, y, esquerda), config->ruido_pct, &sorteio, &rel->leituras_erradas);
        leitura.direita = ler_sensor(parede(lab, x, y, direita), config->ruido_pct, &sorteio, &rel->leituras_erradas);

        nav_acao_t acao = decidir(decisor_ctx, leitura);

        sim_celula_t celula;
        celula_no_robo(&ref, x, y, &celula.x, &celula.y);
        celula.rumo = direcao_no_robo(&ref, rumo);
        celula.acao = acao;
        celula.rumo_depois = direcao_no_robo(&ref, girar(rumo, acao));
        /* Atrás não tem sensor: ou o robô acabou de passar por ali (aberto) ou é o fundo do beco da largada. */
        celula.paredes = (uint8_t)((leitura.frente ? MASCARA_NAV[direcao_no_robo(&ref, rumo)] : 0) |
                                   (leitura.esquerda ? MASCARA_NAV[direcao_no_robo(&ref, esquerda)] : 0) |
                                   (leitura.direita ? MASCARA_NAV[direcao_no_robo(&ref, direita)] : 0) |
                                   (parede(lab, x, y, tras) ? MASCARA_NAV[direcao_no_robo(&ref, tras)] : 0));
        if (config->observador != NULL) {
            config->observador(config->observador_ctx, &celula);
        }
        rel->x = celula.x;
        rel->y = celula.y;

        if (acao == NAV_ACAO_SUCESSO) {
            bool no_objetivo = x == objetivo_x && y == objetivo_y;
            rel->resultado = no_objetivo ? SIM_RESULTADO_SUCESSO : SIM_RESULTADO_SUCESSO_FALSO;
            return;
        }
        if (acao == NAV_ACAO_SEM_PASSO) {
            rel->resultado = SIM_RESULTADO_SEM_CAMINHO;
            return;
        }

        if (acao == NAV_ACAO_DIREITA || acao == NAV_ACAO_ESQUERDA) {
            rel->giros_90++;
        } else if (acao == NAV_ACAO_MEIA_VOLTA) {
            rel->giros_180++;
        }
        rumo = girar(rumo, acao);

        if (parede(lab, x, y, rumo)) {
            rel->resultado = SIM_RESULTADO_COLISAO;
            return;
        }
        if (rel->celulas == limite) {
            rel->resultado = SIM_RESULTADO_LIMITE;
            return;
        }

        x += DX[rumo];
        y += DY[rumo];
        rel->celulas++;
        if (!visitada[x][y]) {
            visitada[x][y] = true;
            rel->distintas++;
        }
    }
}
