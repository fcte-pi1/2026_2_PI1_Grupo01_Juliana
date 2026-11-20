#include "maquina.h"

void maq_iniciar(maq_t *m)
{
    m->estado = MAQ_HEALTH_CHECK;
    m->hc_aprovado = false;
}

/* Parado e fora de tentativa: o health-check novo e os toques do BOOT valem aqui. */
static bool parado_fora_de_tentativa(maq_estado_t e)
{
    return e == MAQ_AGUARDANDO || e == MAQ_REPROVADO || e == MAQ_SUCCESS || e == MAQ_FAILED;
}

maq_acao_t maq_evento(maq_t *m, maq_evento_t evento)
{
    switch (evento) {
    case MAQ_EV_HC_APROVADO:
    case MAQ_EV_HC_REPROVADO:
        if (m->estado != MAQ_HEALTH_CHECK) {
            return MAQ_ACAO_NENHUMA;
        }
        m->hc_aprovado = evento == MAQ_EV_HC_APROVADO;
        m->estado = m->hc_aprovado ? MAQ_AGUARDANDO : MAQ_REPROVADO;
        return MAQ_ACAO_NENHUMA;

    case MAQ_EV_CMD_LARGAR:
        /* Nova execução parte de SUCCESS ou de AGUARDANDO; Retomar, de FAILED. */
        if (!m->hc_aprovado || !parado_fora_de_tentativa(m->estado) || m->estado == MAQ_REPROVADO) {
            return MAQ_ACAO_NENHUMA;
        }
        m->estado = MAQ_RUNNING;
        return MAQ_ACAO_PARTIR;

    case MAQ_EV_CMD_HEALTH_CHECK:
        if (!parado_fora_de_tentativa(m->estado)) {
            return MAQ_ACAO_NENHUMA;
        }
        m->estado = MAQ_HEALTH_CHECK;
        m->hc_aprovado = false; /* só volta a valer com o resultado novo */
        return MAQ_ACAO_RODAR_HEALTH_CHECK;

    case MAQ_EV_CMD_INTERROMPER:
        if (m->estado == MAQ_RUNNING) {
            m->estado = MAQ_FAILED;
            return MAQ_ACAO_PARAR_FALHA_WEB;
        }
        /* Contrato: já parado, só repete a confirmação. */
        return m->estado == MAQ_FAILED ? MAQ_ACAO_REPETIR_FALHA_WEB : MAQ_ACAO_NENHUMA;

    case MAQ_EV_BOOT_TOQUE:
    case MAQ_EV_BOOT_CURTO:
    case MAQ_EV_BOOT_LONGO:
        if (m->estado == MAQ_RUNNING) {
            m->estado = MAQ_FAILED;
            return MAQ_ACAO_PARAR_FALHA_BOOT;
        }
        if (!parado_fora_de_tentativa(m->estado)) {
            return MAQ_ACAO_NENHUMA;
        }
        if (evento == MAQ_EV_BOOT_CURTO) {
            return MAQ_ACAO_APAGAR_MAPA;
        }
        return evento == MAQ_EV_BOOT_LONGO ? MAQ_ACAO_APAGAR_TUDO : MAQ_ACAO_NENHUMA;

    case MAQ_EV_CORRIDA_SUCESSO:
    case MAQ_EV_CORRIDA_FALHA:
        if (m->estado == MAQ_RUNNING) {
            m->estado = evento == MAQ_EV_CORRIDA_SUCESSO ? MAQ_SUCCESS : MAQ_FAILED;
        }
        return MAQ_ACAO_NENHUMA;
    }
    return MAQ_ACAO_NENHUMA;
}

bool maq_falha_da_acao(maq_acao_t acao, uint8_t x, uint8_t y, contrato_falha_t *falha)
{
    switch (acao) {
    case MAQ_ACAO_PARAR_FALHA_WEB:
    case MAQ_ACAO_REPETIR_FALHA_WEB:
        falha->motivo = CONTRATO_MOTIVO_NENHUM;
        falha->origem = CONTRATO_ORIGEM_WEB;
        break;
    case MAQ_ACAO_PARAR_FALHA_BOOT:
        falha->motivo = CONTRATO_MOTIVO_ENCERRADO_OPERADOR;
        falha->origem = CONTRATO_ORIGEM_BOOT;
        break;
    default:
        return false;
    }
    falha->x = x;
    falha->y = y;
    falha->componente = CONTRATO_COMPONENTE_NENHUM;
    return true;
}

contrato_estado_t maq_estado_contrato(const maq_t *m)
{
    switch (m->estado) {
    case MAQ_RUNNING:
        return CONTRATO_ESTADO_RUNNING;
    case MAQ_SUCCESS:
        return CONTRATO_ESTADO_SUCCESS;
    case MAQ_FAILED:
        return CONTRATO_ESTADO_FAILED;
    default:
        return CONTRATO_ESTADO_HEALTH_CHECK;
    }
}

bool maq_andando(const maq_t *m)
{
    return m->estado == MAQ_RUNNING;
}
