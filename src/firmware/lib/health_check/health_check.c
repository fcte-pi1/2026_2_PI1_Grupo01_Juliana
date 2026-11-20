#include "health_check.h"

#include "hal_bateria.h"
#include "dip.h"
#include "hal_dip.h"

static void definir(contrato_hc_item_t *item, contrato_componente_t componente, bool aprovado,
                    bool tem_valor, int32_t valor)
{
    item->componente = componente;
    item->aprovado = aprovado;
    item->tem_valor = tem_valor;
    item->valor = valor;
}

void hc_avaliar(uint16_t bateria_mv, bateria_nivel_t bateria_nivel,
                const tof_autoteste_t tof[TOF_QTD], const int32_t pulsos[MOTOR_QTD],
                contrato_tipo_dip_t tipo_dip, hc_resultado_t *resultado)
{
    int i = 0;
    /* Sem nenhuma leitura (0 mV) a bateria não foi medida: reprova. */
    bool bateria_ok = bateria_mv > 0 && bateria_nivel != BATERIA_BAIXA;
    definir(&resultado->item[i++], CONTRATO_COMPONENTE_BATERIA, bateria_ok, bateria_mv > 0,
            bateria_mv);

    for (int t = 0; t < TOF_QTD; t++) {
        definir(&resultado->item[i++], (contrato_componente_t)(CONTRATO_COMPONENTE_TOF_FRONTAL + t),
                tof[t].aprovado, tof[t].tem_valor, tof[t].valor_mm);
    }

    /* O motor é aprovado pelo encoder da mesma roda. */
    bool roda_esq_ok = motor_autoteste_aprovado(pulsos[MOTOR_ESQ]);
    bool roda_dir_ok = motor_autoteste_aprovado(pulsos[MOTOR_DIR]);
    definir(&resultado->item[i++], CONTRATO_COMPONENTE_MOTOR_ESQUERDO, roda_esq_ok, false, 0);
    definir(&resultado->item[i++], CONTRATO_COMPONENTE_MOTOR_DIREITO, roda_dir_ok, false, 0);
    definir(&resultado->item[i++], CONTRATO_COMPONENTE_ENCODER_ESQUERDO, roda_esq_ok, true,
            pulsos[MOTOR_ESQ]);
    definir(&resultado->item[i++], CONTRATO_COMPONENTE_ENCODER_DIREITO, roda_dir_ok, true,
            pulsos[MOTOR_DIR]);

    resultado->tipo_dip = tipo_dip;
    resultado->dip_valido = tipo_dip != CONTRATO_DIP_INVALIDO;
    resultado->aprovado = resultado->dip_valido;
    resultado->reprovado = CONTRATO_COMPONENTE_NENHUM;
    for (int k = 0; k < HC_ITENS; k++) {
        if (!resultado->item[k].aprovado) {
            resultado->aprovado = false;
            resultado->reprovado = resultado->item[k].componente;
            break;
        }
    }
}

void hc_executar(bateria_t *bateria, hc_esperar_t esperar, hc_resultado_t *resultado)
{
    for (int i = 0; i < BATERIA_MEDIA_AMOSTRAS; i++) {
        bateria_amostrar(bateria, hal_bateria_ler_adc_mv());
    }

    tof_autoteste_t tof[TOF_QTD];
    tof_autoteste_executar(tof);

    int32_t pulsos[MOTOR_QTD];
    motor_autoteste_executar(esperar, pulsos);

    hc_avaliar(bateria_mv(bateria), bateria_nivel(bateria), tof, pulsos,
               dip_tipo(hal_dip_ler()), resultado);
}
