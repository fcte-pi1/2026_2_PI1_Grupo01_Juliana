#include "corrida.h"

#include <string.h>

static void partir_da_pose(corrida_t *c, bool retomada)
{
    c->ultimo_x = retomada ? -1 : c->nav.x;
    c->ultimo_y = retomada ? -1 : c->nav.y;
    c->gravada_x = c->nav.x;
    c->gravada_y = c->nav.y;
    c->terminou = c->nav.terminou;
}

void corrida_iniciar(corrida_t *c, nav_tipo_t tipo_dip)
{
    maq_iniciar(&c->maq);
    c->tipo = tipo_dip;
    c->estado = CONTRATO_ESTADO_HEALTH_CHECK;
    c->inicio = mem_carregar(&c->nav, &c->estado, tipo_dip);
    c->acao = NAV_ACAO_SEM_PASSO;
    partir_da_pose(c, c->inicio == MEM_RETOMADA);
}

static void falha_da_acao(corrida_t *c, maq_acao_t acao, corrida_efeitos_t *ef)
{
    ef->tem_falha = maq_falha_da_acao(acao, (uint8_t)c->nav.x, (uint8_t)c->nav.y, &ef->falha);
}

void corrida_evento(corrida_t *c, maq_evento_t evento, corrida_efeitos_t *ef)
{
    memset(ef, 0, sizeof *ef);
    maq_acao_t acao = maq_evento(&c->maq, evento);

    switch (acao) {
    case MAQ_ACAO_RODAR_HEALTH_CHECK:
        ef->rodar_health_check = true;
        break;
    case MAQ_ACAO_PARTIR:
        if (c->nav.terminou && c->nav.acao_terminal == NAV_ACAO_SUCESSO) {
            nav_iniciar(&c->nav, c->tipo); /* nova execução depois do sucesso */
            partir_da_pose(c, true);
            ef->reposicionar = true;
        }
        c->nav.terminou = false;
        c->terminou = false;
        c->estado = CONTRATO_ESTADO_RUNNING;
        c->acao = NAV_ACAO_SEM_PASSO;
        break;
    case MAQ_ACAO_PARAR_FALHA_WEB:
    case MAQ_ACAO_PARAR_FALHA_BOOT:
        ef->parar_motores = true;
        falha_da_acao(c, acao, ef);
        c->estado = CONTRATO_ESTADO_FAILED;
        mem_gravar(&c->nav, c->estado);
        c->acao = NAV_ACAO_SEM_PASSO;
        break;
    case MAQ_ACAO_REPETIR_FALHA_WEB:
        falha_da_acao(c, acao, ef);
        break;
    case MAQ_ACAO_APAGAR_MAPA:
    case MAQ_ACAO_APAGAR_TUDO:
        if (acao == MAQ_ACAO_APAGAR_MAPA) {
            mem_apagar_mapa();
            ef->bipes = 1;
        } else {
            mem_apagar_tudo();
            ef->bipes = 2;
        }
        nav_iniciar(&c->nav, c->tipo);
        partir_da_pose(c, false);
        ef->reposicionar = true;
        break;
    case MAQ_ACAO_NENHUMA:
        break;
    }
}

void corrida_health_check_concluido(corrida_t *c, bool aprovado, nav_tipo_t tipo_dip,
                                    corrida_efeitos_t *ef)
{
    memset(ef, 0, sizeof *ef);
    maq_evento(&c->maq, aprovado ? MAQ_EV_HC_APROVADO : MAQ_EV_HC_REPROVADO);
    if (tipo_dip != c->tipo) {
        c->tipo = tipo_dip;
        c->inicio = mem_carregar(&c->nav, &c->estado, tipo_dip);
        partir_da_pose(c, c->inicio == MEM_RETOMADA);
        ef->reposicionar = true;
    }
}

int corrida_decidir(corrida_t *c, nav_leitura_t leitura, bool *reposicionar,
                    telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX])
{
    *reposicionar = false;
    if (!maq_andando(&c->maq)) {
        return 0;
    }

    /* A leitura é da célula atual; a pose é guardada antes de o passo levá-la à próxima. */
    int8_t x = c->nav.x;
    int8_t y = c->nav.y;
    nav_direcao_t direcao = c->nav.direcao;

    bool ja_acabou = c->terminou;
    c->acao = mem_aplicar_leitura(&c->nav, &c->estado, &c->gravada_x, &c->gravada_y,
                                  &c->terminou, leitura);
    *reposicionar = c->nav.x != x || c->nav.y != y || c->nav.direcao != direcao;

    telemetria_decisao_t d = {0};
    d.mudou_celula = x != c->ultimo_x || y != c->ultimo_y;
    d.x = x;
    d.y = y;
    d.paredes = telemetria_paredes_do_nav(direcao, c->nav.lado, leitura);
    d.terminou_agora = !ja_acabou && c->terminou;
    d.acao = c->acao;
    d.nav_x = c->nav.x;
    d.nav_y = c->nav.y;
    int qtd = telemetria_eventos_da_decisao(&d, saida);

    if (d.mudou_celula) {
        c->ultimo_x = x;
        c->ultimo_y = y;
    }
    if (d.terminou_agora) {
        maq_evento(&c->maq, c->acao == NAV_ACAO_SUCESSO ? MAQ_EV_CORRIDA_SUCESSO
                                                         : MAQ_EV_CORRIDA_FALHA);
    }
    return qtd;
}

contrato_estado_t corrida_estado_contrato(const corrida_t *c)
{
    return maq_estado_contrato(&c->maq);
}

bool corrida_em_movimento(const corrida_t *c)
{
    return maq_andando(&c->maq) && c->acao != NAV_ACAO_SUCESSO && c->acao != NAV_ACAO_SEM_PASSO;
}
