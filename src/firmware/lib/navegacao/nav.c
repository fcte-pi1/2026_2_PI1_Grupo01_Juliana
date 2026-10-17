#include "nav.h"

#include <string.h>

/* Lado curto de todos os tipos: 4 células. */
#define NAV_LADO_CURTO 4

static const uint8_t PAREDE_DA_DIRECAO[4] = {
    NAV_PAREDE_N, NAV_PAREDE_L, NAV_PAREDE_S, NAV_PAREDE_O
};
static const int8_t PASSO_X[4] = {0, 1, 0, -1};
static const int8_t PASSO_Y[4] = {1, 0, -1, 0};

static nav_direcao_t girar_horario(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 1) % 4);
}

static nav_direcao_t girar_anti_horario(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 3) % 4);
}

static nav_direcao_t oposta(nav_direcao_t direcao)
{
    return (nav_direcao_t)((direcao + 2) % 4);
}

static int lado_longo(nav_tipo_t tipo)
{
    switch (tipo) {
    case NAV_TIPO_8X4:
        return 8;
    case NAV_TIPO_12X4:
        return 12;
    default:
        return NAV_LADO_CURTO;
    }
}

/*
 * Região válida (D6). Enquanto a orientação não é resolvida, vale a união das
 * duas: um "L" com um braço em x e outro em y.
 */
static bool na_regiao(const nav_estado_t *estado, int x, int y)
{
    if (x < 0 || y < 0) {
        return false;
    }
    int longo = lado_longo(estado->tipo);
    bool longo_x = x < longo && y < NAV_LADO_CURTO;
    bool longo_y = x < NAV_LADO_CURTO && y < longo;
    switch (estado->orientacao) {
    case NAV_ORIENTACAO_LONGO_X:
        return longo_x;
    case NAV_ORIENTACAO_LONGO_Y:
        return longo_y;
    default:
        return longo_x || longo_y;
    }
}

static bool na_matriz(int x, int y)
{
    return x >= 0 && y >= 0 && x < NAV_MAX_CELULAS && y < NAV_MAX_CELULAS;
}

/*
 * Votos da parede de (x, y) na direção dada: a sul e a oeste são a norte e a
 * leste da vizinha. NULL quando a parede fica fora da matriz, que é borda.
 */
static nav_parede_t *parede_de(nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    if (direcao == NAV_SUL) {
        y--;
        direcao = NAV_NORTE;
    } else if (direcao == NAV_OESTE) {
        x--;
        direcao = NAV_LESTE;
    }
    if (!na_matriz(x, y)) {
        return NULL;
    }
    return &estado->paredes[x][y][direcao == NAV_NORTE ? 0 : 1];
}

/* Regra da D11. */
static bool parede_existe(const nav_parede_t *parede)
{
    if (parede->trocas & NAV_BORDA) {
        return true;
    }
    if (parede->trocas & NAV_PROVADA_ABERTA) {
        return false;
    }
    return parede->votos_parede > parede->votos_aberto;
}

/* Põe ou tira a parede do mapa, na célula e no lado oposto da vizinha. */
static void aplicar_parede(nav_estado_t *estado, int x, int y, nav_direcao_t direcao, bool existe)
{
    int vx = x + PASSO_X[direcao];
    int vy = y + PASSO_Y[direcao];
    if (existe) {
        estado->mapa[x][y].paredes |= PAREDE_DA_DIRECAO[direcao];
        if (na_matriz(vx, vy)) {
            estado->mapa[vx][vy].paredes |= PAREDE_DA_DIRECAO[oposta(direcao)];
        }
    } else {
        estado->mapa[x][y].paredes &= (uint8_t)~PAREDE_DA_DIRECAO[direcao];
        if (na_matriz(vx, vy)) {
            estado->mapa[vx][vy].paredes &= (uint8_t)~PAREDE_DA_DIRECAO[oposta(direcao)];
        }
    }
    estado->mapa_alterado = true;
}

static bool parede_no_mapa(const nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    return (estado->mapa[x][y].paredes & PAREDE_DA_DIRECAO[direcao]) != 0;
}

/*
 * Uma leitura soma 1 voto. Se a parede mudar de estado, conta uma troca;
 * depois de NAV_TROCAS_MAX trocas, as leituras só somam votos (D13).
 */
static void votar(nav_estado_t *estado, int x, int y, nav_direcao_t direcao, bool parede)
{
    nav_parede_t *p = parede_de(estado, x, y, direcao);
    if (p == NULL) {
        return;
    }
    uint8_t *votos = parede ? &p->votos_parede : &p->votos_aberto;
    if (*votos < 255) {
        (*votos)++;
    }
    bool existe = parede_existe(p);
    if (existe == parede_no_mapa(estado, x, y, direcao)) {
        return;
    }
    if ((p->trocas & NAV_TROCAS_MASCARA) >= NAV_TROCAS_MAX) {
        return;
    }
    p->trocas++;
    aplicar_parede(estado, x, y, direcao, existe);
}

/*
 * Voto da conferência da frente (D15): o robô parado viu a parede, então ela
 * muda o mapa mesmo depois de NAV_TROCAS_MAX trocas.
 */
static void votar_bloqueio(nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    nav_parede_t *p = parede_de(estado, x, y, direcao);
    if (p == NULL) {
        return;
    }
    if (p->votos_parede < 255) {
        p->votos_parede++;
    }
    bool existe = parede_existe(p);
    if (existe == parede_no_mapa(estado, x, y, direcao)) {
        return;
    }
    if ((p->trocas & NAV_TROCAS_MASCARA) < NAV_TROCAS_MASCARA) {
        p->trocas++;
    }
    aplicar_parede(estado, x, y, direcao, existe);
}

/* O robô passou pela parede: ela fica aberta para sempre (D12). */
static void provar_aberta(nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    nav_parede_t *p = parede_de(estado, x, y, direcao);
    if (p == NULL) {
        return;
    }
    p->trocas |= NAV_PROVADA_ABERTA;
    if (parede_no_mapa(estado, x, y, direcao)) {
        aplicar_parede(estado, x, y, direcao, false);
    }
}

/*
 * Perdão (D14): esquece as paredes do mapa com vantagem de no máximo 1 voto,
 * igualando os votos de parede aos de aberto, sem contar troca. Borda e
 * provada aberta ficam como estão. Devolve se alguma parede saiu do mapa.
 */
static bool perdoar(nav_estado_t *estado)
{
    bool perdoou = false;
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            for (int i = 0; i < 2; i++) {
                nav_direcao_t direcao = i == 0 ? NAV_NORTE : NAV_LESTE;
                nav_parede_t *p = &estado->paredes[x][y][i];
                if (p->trocas & (NAV_BORDA | NAV_PROVADA_ABERTA)) {
                    continue;
                }
                if (!parede_no_mapa(estado, x, y, direcao) || p->votos_parede > p->votos_aberto + 1) {
                    continue;
                }
                if (p->votos_parede > p->votos_aberto) {
                    p->votos_parede = p->votos_aberto;
                }
                aplicar_parede(estado, x, y, direcao, false);
                perdoou = true;
            }
        }
    }
    return perdoou;
}

/*
 * Recalcula o flood fill e, se a célula atual não alcança nenhum objetivo,
 * perdoa e tenta de novo. Devolve se há caminho.
 */
static bool recalcular_com_perdao(nav_estado_t *estado)
{
    nav_recalcular(estado);
    while (estado->mapa[estado->x][estado->y].distancia == NAV_DIST_INFINITA) {
        if (estado->perdoes >= NAV_PERDOES_MAX || !perdoar(estado)) {
            return false;
        }
        estado->perdoes++;
        nav_recalcular(estado);
    }
    return true;
}

/*
 * Na coluna x = 0, a primeira abertura lateral diz para que lado do robô fica
 * o x positivo (D5). As laterais são vistas como na largada: com rumo sul, a
 * direita física é o lado que era a esquerda.
 */
static void fixar_lado(nav_estado_t *estado, nav_leitura_t leitura)
{
    if (estado->lado != NAV_LADO_INDEFINIDO || estado->x != 0) {
        return;
    }
    bool rumo_norte = estado->direcao == NAV_NORTE;
    if (!rumo_norte && estado->direcao != NAV_SUL) {
        return;
    }
    bool aberta_direita_da_largada = rumo_norte ? !leitura.direita : !leitura.esquerda;
    bool aberta_esquerda_da_largada = rumo_norte ? !leitura.esquerda : !leitura.direita;
    if (aberta_direita_da_largada) {
        estado->lado = NAV_LADO_DIREITA;
    } else if (aberta_esquerda_da_largada) {
        estado->lado = NAV_LADO_ESQUERDA;
    } else {
        return;
    }
    /* Provisório até o robô sair da coluna 0: a abertura pode ser falsa (D16). */
    estado->lado_provisorio = true;
    estado->lado_y = estado->y;
    estado->lado_duas_abertas = aberta_direita_da_largada && aberta_esquerda_da_largada;
}

/*
 * Sair da coluna 0 torna o lado definitivo (D17). Indefinido vira direita, o
 * espelho que a navegação já vinha usando.
 */
static void tornar_lado_definitivo(nav_estado_t *estado)
{
    if (estado->x == 0) {
        return;
    }
    estado->lado_provisorio = false;
    if (estado->lado == NAV_LADO_INDEFINIDO) {
        estado->lado = NAV_LADO_DIREITA;
    }
}

/*
 * Lado provisório (D16): a conferência viu parede no leste a partir da célula
 * onde o lado foi fixado, então a abertura que o fixou era falsa. O robô está
 * virado para o lado físico que se achava o leste. Devolve se desfez o lado.
 */
static bool desfazer_lado(nav_estado_t *estado)
{
    if (!estado->lado_provisorio || estado->desfazeres >= NAV_DESFAZERES_MAX) {
        return false;
    }
    if (estado->x != 0 || estado->y != estado->lado_y || estado->direcao != NAV_LESTE) {
        return false;
    }
    estado->desfazeres++;
    /* As paredes leste da coluna 0 foram gravadas com o espelho errado. */
    for (int y = 0; y < NAV_MAX_CELULAS; y++) {
        memset(&estado->paredes[0][y][1], 0, sizeof(nav_parede_t));
        if (parede_no_mapa(estado, 0, y, NAV_LESTE)) {
            aplicar_parede(estado, 0, y, NAV_LESTE, false);
        }
    }
    if (estado->lado_duas_abertas) {
        /*
         * A outra lateral também estava aberta: o x positivo fica do outro
         * lado, e o lado físico para onde o robô olha passa a ser o oeste.
         */
        estado->lado = estado->lado == NAV_LADO_DIREITA ? NAV_LADO_ESQUERDA : NAV_LADO_DIREITA;
        estado->lado_duas_abertas = false;
        estado->direcao = NAV_OESTE;
        return true;
    }
    /*
     * Só um lado aberto, e ele era falso: o lado volta a indefinido, que
     * espelha como direita. Se era direita, o robô olha para o leste, e a
     * parede que ele viu ganha 2 votos; se era esquerda, olha para o oeste,
     * que é borda, e o leste é a direita física, lida fechada na mesma
     * leitura (gravada como oeste e perdida na borda): ganha 1 voto.
     */
    bool olha_leste = estado->lado == NAV_LADO_DIREITA;
    estado->lado = NAV_LADO_INDEFINIDO;
    estado->lado_provisorio = false;
    if (olha_leste) {
        votar(estado, 0, estado->y, NAV_LESTE, true);
        votar(estado, 0, estado->y, NAV_LESTE, true);
    } else {
        estado->direcao = NAV_OESTE;
        votar(estado, 0, estado->y, NAV_LESTE, true);
    }
    return true;
}

/* No 8x4 e no 12x4, pisar fora do quadrado 4x4 da largada resolve a orientação. */
static void resolver_orientacao(nav_estado_t *estado)
{
    if (estado->tipo == NAV_TIPO_4X4 || estado->orientacao != NAV_ORIENTACAO_INDEFINIDA) {
        return;
    }
    if (estado->x >= NAV_LADO_CURTO) {
        estado->orientacao = NAV_ORIENTACAO_LONGO_X;
        estado->mapa_alterado = true;
    } else if (estado->y >= NAV_LADO_CURTO) {
        estado->orientacao = NAV_ORIENTACAO_LONGO_Y;
        estado->mapa_alterado = true;
    }
}

/*
 * Passa a leitura, relativa à frente do robô, para o referencial do labirinto.
 * Com o lado indefinido, as duas laterais estão fechadas (senão o lado teria
 * sido fixado), e tanto faz o espelho: viram as paredes leste e oeste.
 */
static void registrar_leitura(nav_estado_t *estado, nav_leitura_t leitura)
{
    fixar_lado(estado, leitura);

    nav_direcao_t direita = girar_horario(estado->direcao);
    nav_direcao_t esquerda = girar_anti_horario(estado->direcao);
    if (estado->lado == NAV_LADO_ESQUERDA) {
        direita = girar_anti_horario(estado->direcao);
        esquerda = girar_horario(estado->direcao);
    }

    int x = estado->x;
    int y = estado->y;
    votar(estado, x, y, estado->direcao, leitura.frente);
    votar(estado, x, y, direita, leitura.direita);
    votar(estado, x, y, esquerda, leitura.esquerda);

    estado->mapa[x][y].paredes |= NAV_VISITADA;
    resolver_orientacao(estado);
}

void nav_iniciar(nav_estado_t *estado, nav_tipo_t tipo)
{
    memset(estado, 0, sizeof(*estado));
    estado->x = 0;
    estado->y = 0;
    estado->direcao = NAV_NORTE;
    estado->tipo = tipo;
    estado->lado = NAV_LADO_INDEFINIDO;
    estado->orientacao = NAV_ORIENTACAO_INDEFINIDA;
    /*
     * Do lado -x e atrás da largada só existe borda. Essas paredes ficam fora
     * da matriz de votos; as da outra ponta da matriz são marcadas como borda.
     */
    int ultima = NAV_MAX_CELULAS - 1;
    for (int i = 0; i < NAV_MAX_CELULAS; i++) {
        estado->mapa[i][0].paredes |= NAV_PAREDE_S;
        estado->mapa[0][i].paredes |= NAV_PAREDE_O;
        estado->mapa[i][ultima].paredes |= NAV_PAREDE_N;
        estado->mapa[ultima][i].paredes |= NAV_PAREDE_L;
        estado->paredes[i][ultima][0].trocas = NAV_BORDA;
        estado->paredes[ultima][i][1].trocas = NAV_BORDA;
    }
    nav_recalcular(estado);
}

bool nav_acessivel(const nav_estado_t *estado, int x, int y, nav_direcao_t direcao)
{
    if (!na_regiao(estado, x, y)) {
        return false;
    }
    if (estado->mapa[x][y].paredes & PAREDE_DA_DIRECAO[direcao]) {
        return false;
    }
    return na_regiao(estado, x + PASSO_X[direcao], y + PASSO_Y[direcao]);
}

uint8_t nav_distancia(const nav_estado_t *estado, int x, int y)
{
    if (x < 0 || y < 0 || x >= NAV_MAX_CELULAS || y >= NAV_MAX_CELULAS) {
        return NAV_DIST_INFINITA;
    }
    return estado->mapa[x][y].distancia;
}

nav_orientacao_t nav_orientacao(const nav_estado_t *estado)
{
    return estado->orientacao;
}

/*
 * Objetivos (célula de chegada) de cada tipo. Um objetivo fora da região atual
 * não vale: assim o incompatível sai quando a orientação é resolvida (D6).
 */
static bool eh_objetivo(const nav_estado_t *estado, int x, int y)
{
    if (!na_regiao(estado, x, y)) {
        return false;
    }
    int ultimo_longo = lado_longo(estado->tipo) - 1;
    int ultimo_curto = NAV_LADO_CURTO - 1;
    return (x == ultimo_longo && y == ultimo_curto) || (x == ultimo_curto && y == ultimo_longo);
}

/*
 * Flood fill de Harrison [9], como na seção "Algoritmo de navegação" do docs/4.4
 * (D7): objetivos em 0, as demais células em infinito, e varreduras da matriz
 * inteira fazendo valor = 1 + menor valor entre as vizinhas acessíveis, até uma
 * varredura não mudar nada. Parede desconhecida conta como aberta (Adachi).
 * Sem fila e sem pilha, de propósito.
 */
void nav_recalcular(nav_estado_t *estado)
{
    for (int x = 0; x < NAV_MAX_CELULAS; x++) {
        for (int y = 0; y < NAV_MAX_CELULAS; y++) {
            estado->mapa[x][y].distancia = eh_objetivo(estado, x, y) ? 0 : NAV_DIST_INFINITA;
        }
    }

    bool mudou = true;
    while (mudou) {
        mudou = false;
        for (int x = 0; x < NAV_MAX_CELULAS; x++) {
            for (int y = 0; y < NAV_MAX_CELULAS; y++) {
                if (!na_regiao(estado, x, y) || eh_objetivo(estado, x, y)) {
                    continue;
                }
                uint8_t menor = NAV_DIST_INFINITA;
                for (int d = NAV_NORTE; d <= NAV_OESTE; d++) {
                    if (!nav_acessivel(estado, x, y, (nav_direcao_t)d)) {
                        continue;
                    }
                    uint8_t vizinha = estado->mapa[x + PASSO_X[d]][y + PASSO_Y[d]].distancia;
                    if (vizinha < menor) {
                        menor = vizinha;
                    }
                }
                uint8_t valor = menor == NAV_DIST_INFINITA ? NAV_DIST_INFINITA : (uint8_t)(menor + 1);
                if (valor != estado->mapa[x][y].distancia) {
                    estado->mapa[x][y].distancia = valor;
                    mudou = true;
                }
            }
        }
    }
    estado->mapa_alterado = false;
}

static nav_acao_t terminar(nav_estado_t *estado, nav_acao_t acao)
{
    estado->terminou = true;
    estado->acao_terminal = acao;
    return acao;
}

/*
 * Vizinha acessível de menor valor. No empate (D9) vence a primeira desta
 * ordem: frente, giro horário no referencial do labirinto, anti-horário e
 * meia-volta. Assim as versões espelhadas de um labirinto têm o mesmo trajeto.
 */
static nav_direcao_t escolher_direcao(const nav_estado_t *estado)
{
    nav_direcao_t atual = estado->direcao;
    const nav_direcao_t ordem[4] = {
        atual, girar_horario(atual), girar_anti_horario(atual), oposta(atual)
    };
    nav_direcao_t escolhida = atual;
    uint8_t menor = NAV_DIST_INFINITA;
    for (int i = 0; i < 4; i++) {
        nav_direcao_t d = ordem[i];
        if (!nav_acessivel(estado, estado->x, estado->y, d)) {
            continue;
        }
        uint8_t valor = estado->mapa[estado->x + PASSO_X[d]][estado->y + PASSO_Y[d]].distancia;
        if (valor < menor) {
            menor = valor;
            escolhida = d;
        }
    }
    return escolhida;
}

/* Ação física: com o x positivo à esquerda do robô, os giros trocam (D5). */
static nav_acao_t acao_para(const nav_estado_t *estado, nav_direcao_t destino)
{
    nav_direcao_t atual = estado->direcao;
    if (destino == atual) {
        return NAV_ACAO_FRENTE;
    }
    if (destino == oposta(atual)) {
        return NAV_ACAO_MEIA_VOLTA;
    }
    bool horario = destino == girar_horario(atual);
    if (estado->lado == NAV_LADO_ESQUERDA) {
        horario = !horario;
    }
    return horario ? NAV_ACAO_DIREITA : NAV_ACAO_ESQUERDA;
}

/* Supõe que o app executa a ação: o estado já fica na célula de destino (D4). */
static nav_acao_t mover(nav_estado_t *estado)
{
    nav_direcao_t destino = escolher_direcao(estado);
    nav_acao_t acao = acao_para(estado, destino);
    estado->direcao = destino;
    estado->x = (int8_t)(estado->x + PASSO_X[destino]);
    estado->y = (int8_t)(estado->y + PASSO_Y[destino]);
    estado->movimento_pendente = true;
    return acao;
}

nav_acao_t nav_passo(nav_estado_t *estado, nav_leitura_t leitura)
{
    /* Depois do fim, a mesma resposta e nenhuma mudança no estado (D10). */
    if (estado->terminou) {
        return estado->acao_terminal;
    }
    /* Pela D4, a origem do movimento é a célula atual menos o passo do rumo. */
    if (estado->movimento_pendente) {
        estado->movimento_pendente = false;
        provar_aberta(estado, estado->x, estado->y, oposta(estado->direcao));
    }
    tornar_lado_definitivo(estado);
    registrar_leitura(estado, leitura);
    if (eh_objetivo(estado, estado->x, estado->y)) {
        return terminar(estado, NAV_ACAO_SUCESSO);
    }
    /*
     * Sem parede mudada nem orientação resolvida, os valores seriam os mesmos
     * (D8). Sem caminho, o recálculo perdoa as paredes duvidosas (D14).
     */
    bool sem_caminho = estado->mapa[estado->x][estado->y].distancia == NAV_DIST_INFINITA;
    if ((estado->mapa_alterado || sem_caminho) && !recalcular_com_perdao(estado)) {
        return terminar(estado, NAV_ACAO_SEM_PASSO);
    }
    return mover(estado);
}

nav_acao_t nav_bloqueado(nav_estado_t *estado)
{
    if (estado->terminou) {
        return estado->acao_terminal;
    }
    if (!estado->movimento_pendente) {
        return NAV_ACAO_SEM_PASSO;
    }
    /* Garantia de término: a conferência não pode girar para sempre. */
    if (estado->bloqueios >= NAV_BLOQUEIOS_MAX) {
        return terminar(estado, NAV_ACAO_SEM_PASSO);
    }
    estado->bloqueios++;
    /* O robô não andou: volta à origem e fica com o rumo novo (D4). */
    estado->x = (int8_t)(estado->x - PASSO_X[estado->direcao]);
    estado->y = (int8_t)(estado->y - PASSO_Y[estado->direcao]);
    if (!desfazer_lado(estado)) {
        votar_bloqueio(estado, estado->x, estado->y, estado->direcao);
    }
    if (estado->mapa_alterado && !recalcular_com_perdao(estado)) {
        return terminar(estado, NAV_ACAO_SEM_PASSO);
    }
    return mover(estado);
}

nav_direcao_t nav_direcao_apos(nav_direcao_t atual, nav_acao_t acao)
{
    switch (acao) {
    case NAV_ACAO_DIREITA:
        return (nav_direcao_t)((atual + 1) % 4);
    case NAV_ACAO_ESQUERDA:
        return (nav_direcao_t)((atual + 3) % 4);
    case NAV_ACAO_MEIA_VOLTA:
        return (nav_direcao_t)((atual + 2) % 4);
    default:
        return atual;
    }
}
