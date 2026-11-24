#include "execucao.h"

#include "config/robo.h"

static struct {
    exec_estado_t estado;
    int giro;        /* graus da etapa de giro; 0 = sem giro */
    bool lateral_lida; /* a leitura lateral da ação já foi pedida */
} exec;

void exec_iniciar(void)
{
    exec.estado = EXEC_OCIOSO;
    exec.giro = 0;
    exec.lateral_lida = false;
}

bool exec_eh_movimento(nav_acao_t acao)
{
    return acao == NAV_ACAO_FRENTE || acao == NAV_ACAO_DIREITA || acao == NAV_ACAO_ESQUERDA ||
           acao == NAV_ACAO_MEIA_VOLTA;
}

static int giro_da_acao(nav_acao_t acao)
{
    switch (acao) {
    case NAV_ACAO_DIREITA:
        return -90;
    case NAV_ACAO_ESQUERDA:
        return 90;
    case NAV_ACAO_MEIA_VOLTA:
        return 180;
    default:
        return 0;
    }
}

bool exec_comecar(nav_acao_t acao)
{
    if (!exec_eh_movimento(acao) || exec.estado == EXEC_GIRANDO || exec.estado == EXEC_AVANCANDO) {
        return false;
    }
    exec.giro = giro_da_acao(acao);
    exec.lateral_lida = false;
    if (exec.giro != 0) {
        if (!mov_girar(exec.giro)) {
            return false;
        }
        exec.estado = EXEC_GIRANDO;
    } else {
        if (!mov_avancar_celula()) {
            return false;
        }
        exec.estado = EXEC_AVANCANDO;
    }
    return true;
}

exec_estado_t exec_atualizar(void)
{
    if (exec.estado != EXEC_GIRANDO && exec.estado != EXEC_AVANCANDO) {
        return exec.estado;
    }
    mov_estado_t m = mov_atualizar();
    if (m == MOV_FALHOU) {
        exec.estado = EXEC_FALHOU;
    } else if (m == MOV_CONCLUIDO) {
        if (exec.estado == EXEC_GIRANDO) {
            exec.estado = mov_avancar_celula() ? EXEC_AVANCANDO : EXEC_FALHOU;
        } else {
            exec.estado = EXEC_OCIOSO;
        }
    }
    return exec.estado;
}

bool exec_ler_lateral_agora(void)
{
    if (exec.estado != EXEC_AVANCANDO || exec.lateral_lida ||
        mov_andado_mm() < (float)EXEC_LEITURA_LATERAL_MM) {
        return false;
    }
    exec.lateral_lida = true;
    return true;
}

exec_estado_t exec_estado(void)
{
    return exec.estado;
}

void exec_parar(void)
{
    mov_parar();
    exec.estado = EXEC_OCIOSO;
    exec.giro = 0;
}
