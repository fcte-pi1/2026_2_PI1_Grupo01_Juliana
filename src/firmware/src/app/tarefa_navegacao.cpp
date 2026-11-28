/*
 * Tarefa de navegação (núcleo 1): lê os sensores, decide, anda e publica para a telemetria a amostra
 * da tel e os eventos do contrato.
 *
 * O laço roda a cada MOV_PERIODO_MS (5 ms), o ciclo do controle de movimento. Anda e para: parado no
 * centro de uma célula, espera os ToF estabilizarem, decide a ação e a executa (giro e avanço pelo
 * lib/execucao). As paredes são lidas com o robô parado. Com a
 * HAL simulada o robô anda pela física do mundo simulado, e com a real, pelos motores.
 *
 * No boot, carrega o mapa da NVS, publica o health-check (hc_item e hc_resultado, com inicio
 * "retomada" quando o mapa voltou) e espera o comando largar; só parte com o último health-check
 * aprovado. A cada célula nova, grava de novo. Sucesso e falha saem uma vez, quando a corrida acaba.
 * Interromper, health_check e os toques do BOOT passam pela máquina de estados.
 */

#include <Arduino.h>
#include <stdio.h>

#include "app.h"
#include "bateria.h"
#include "botao.h"
#include "config/robo.h"
#include "corrida.h"
#include "eventos.h"
#include "execucao.h"
#include "dip.h"
#include "hal_bateria.h"
#include "hal_boot.h"
#include "hal_dip.h"
#include "hal_led.h"
#include "hal_motor.h"
#include "hal_tempo.h"
#include "hal_tof.h"
#include "health_check.h"
#include "maquina.h"
#include "movimento.h"
#include "nav.h"
#include "sensores.h"
#include "sinal.h"
#include "telemetria_nav.h"

#ifdef HAL_SIMULADA
#include "sim_mundo.h"
#endif

#define PERIODO_MS              MOV_PERIODO_MS  /* ciclo do controle de movimento */
#define CICLOS_POR_LEITURA      10   /* 50 ms: leitura dos ToF a 20 Hz (FIRM-03) */
#define CICLOS_POR_AMOSTRA      40   /* 200 ms: bateria e tel a 5 Hz; a taxa da tel quem decide é a telemetria */
#define ESPERA_PARADO_MS        200  /* parado, a mediana dos ToF precisa de amostras novas */

static void publicar_falha(contrato_motivo_t motivo, int8_t x, int8_t y,
                           contrato_componente_t componente)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_FALHA;
    ev.m.falha.motivo = motivo;
    ev.m.falha.origem = CONTRATO_ORIGEM_AUTOMATICA;
    ev.m.falha.x = (uint8_t)x;
    ev.m.falha.y = (uint8_t)y;
    ev.m.falha.componente = componente;
    app_publicar_evento(&ev);
}

static void publicar_falha_pronta(const contrato_falha_t *falha)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_FALHA;
    ev.m.falha = *falha;
    app_publicar_evento(&ev);
}

static void bipe(int vezes)
{
    for (int i = 0; i < vezes; i++) {
        hal_buzzer_definir(true);
        hal_led_definir(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        hal_buzzer_definir(false);
        hal_led_definir(false);
        if (i + 1 < vezes) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

static void publicar_sucesso(int8_t x, int8_t y)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_SUCESSO;
    ev.m.sucesso.x = (uint8_t)x;
    ev.m.sucesso.y = (uint8_t)y;
    app_publicar_evento(&ev);
}

static void publicar_passo(int8_t x, int8_t y, uint8_t paredes)
{
    telemetria_evento_t ev = {};
    ev.tipo = TELEMETRIA_EVENTO_PASSO;
    ev.m.passo.x = (uint8_t)x;
    ev.m.passo.y = (uint8_t)y;
    ev.m.passo.paredes = paredes;
    app_publicar_evento(&ev);
}

/* Espera do autoteste dos motores: a tarefa dorme, e a simulação deixa o mundo andar. */
static void esperar_ms(uint32_t ms)
{
#ifdef HAL_SIMULADA
    for (uint32_t t = 0; t < ms; t += PERIODO_MS) {
        sim_mundo_avancar(PERIODO_MS);
        vTaskDelay(pdMS_TO_TICKS(PERIODO_MS));
    }
#else
    vTaskDelay(pdMS_TO_TICKS(ms));
#endif
}

/*
 * Roda o health-check (8 itens) e publica um hc_item por componente e o
 * hc_resultado. Reprovação não gera falha: vai só nesses eventos.
 * Devolve se foi aprovado.
 */
static bool publicar_health_check(mem_inicio_t inicio, bateria_t *bateria)
{
    hc_resultado_t resultado;
    mov_parar(); /* o autoteste gira as rodas: o movimento tem que estar parado */
#ifdef HAL_SIMULADA
    sim_definir_rodas_no_ar(true); /* como no robô real durante o autoteste */
#endif
    hc_executar(bateria, esperar_ms, &resultado);
#ifdef HAL_SIMULADA
    sim_definir_rodas_no_ar(false);
#endif
    telemetria_evento_t eventos[HC_EVENTOS];
    int n = hc_montar_eventos(&resultado,
                              inicio == MEM_RETOMADA ? CONTRATO_INICIO_RETOMADA : CONTRATO_INICIO_NOVA,
                              eventos);
    for (int i = 0; i < n; i++) {
        app_publicar_evento(&eventos[i]);
    }
    return resultado.aprovado;
}

#ifdef HAL_SIMULADA
static void acompanhar_celula(const nav_estado_t *nav)
{
    sim_mundo_no_centro_da_celula(nav->x, nav->y, (int)nav->direcao);
}
#endif

/*
 * Diagnóstico na pista: ao fim de cada etapa de movimento, uma linha "# mov" com o que o robô fez e estimou; a
 * cada decisão, uma linha "# dec" com o que ele viu. A ponte grava no log e não manda ao backend.
 */
/* Um ajuste por parada: o frontal filtrado diz se o robô ficou para trás na célula. */
static bool aproximar_da_parede(void)
{
    uint16_t frente = 0;
    return sensores_distancia_mm(TOF_FRONTAL, &frente) && exec_aproximar_da_parede(frente);
}

static void publicar_diagnostico_movimento(void)
{
    mov_diag_t d;
    if (!mov_diagnostico(&d)) {
        return;
    }
    char linha[APP_DIAG_TAMANHO];
    snprintf(linha, sizeof(linha),
             "# mov t=%lu %c pedido=%.1f fim=%s dur=%lums esq=%ld/%ld dir=%ld/%ld lat=%.1f rumo=%.1f deriva=%.2f"
             " tof=%u/%u/%u\n",
             (unsigned long)hal_tempo_ms(), d.tipo, (double)d.pedido, mov_fim_nome(d.fim),
             (unsigned long)d.duracao_ms, (long)d.pulsos_esq, (long)d.alvo_esq, (long)d.pulsos_dir,
             (long)d.alvo_dir, (double)d.lateral_mm, (double)d.rumo_graus, (double)d.deriva_graus_m,
             (unsigned)d.tof.frente_mm, (unsigned)d.tof.esquerda_mm, (unsigned)d.tof.direita_mm);
    app_publicar_diagnostico(linha);
}

/* A ação leva o robô para um lado onde o sensor vê parede agora? (De ré não há sensor.) */
static bool vai_contra_parede(nav_acao_t acao, nav_leitura_t leitura)
{
    return (acao == NAV_ACAO_FRENTE && leitura.frente) || (acao == NAV_ACAO_ESQUERDA && leitura.esquerda) ||
           (acao == NAV_ACAO_DIREITA && leitura.direita);
}

static void publicar_diagnostico_decisao(const nav_estado_t *nav, nav_leitura_t leitura, nav_acao_t acao,
                                         uint16_t bateria_mv)
{
    static const char *const ACOES[] = {"frente", "direita", "esquerda", "meia_volta", "sucesso", "sem_passo", "re"};
    static const char RUMOS[] = {'N', 'L', 'S', 'O'};
    uint16_t f = 0, e = 0, d = 0;
    sensores_distancia_mm(TOF_FRONTAL, &f);
    sensores_distancia_mm(TOF_ESQUERDO, &e);
    sensores_distancia_mm(TOF_DIREITO, &d);
    char linha[APP_DIAG_TAMANHO];
    snprintf(linha, sizeof(linha),
             "# dec t=%lu proxima=%d,%d rumo=%c paredes F%d E%d D%d tof=%u/%u/%u acao=%s"
             " lat=%.1f rumo_est=%.1f bat=%u\n",
             (unsigned long)hal_tempo_ms(), nav->x, nav->y, RUMOS[(int)nav->direcao & 3], leitura.frente,
             leitura.esquerda, leitura.direita, f, e, d,
             (unsigned)acao < 7 ? ACOES[acao] : "?", (double)mov_lateral_mm(),
             (double)(mov_rumo_rad() * 57.2958f), (unsigned)bateria_mv);
    app_publicar_diagnostico(linha);
}

/* Distâncias filtradas e paredes dos ToF para as correções do movimento. */
static void informar_tof_ao_movimento(void)
{
    paredes_t p = sensores_paredes();
    uint16_t frente = 0, esquerda = 0, direita = 0;
    sensores_distancia_mm(TOF_FRONTAL, &frente);
    sensores_distancia_mm(TOF_ESQUERDO, &esquerda);
    sensores_distancia_mm(TOF_DIREITO, &direita);
    mov_tof_t tof = {p.frente, p.esquerda, p.direita, frente, esquerda, direita};
    mov_informar_tof(&tof);
}

void tarefa_navegacao(void *parametro)
{
    (void)parametro;
    /* Estático: com cerca de 1,2 KB, o estado não deve ocupar a pilha de 4 KB da tarefa. */
    static corrida_t corrida;
    static botao_t botao;
    static sinal_t sinal;
    bateria_t bateria;
    bateria_iniciar(&bateria);
    botao_iniciar(&botao);
    sinal_iniciar(&sinal);
    exec_iniciar();
    nav_permitir_re(true); /* beco no fim de uma reta: volta de ré em vez de girar 180° */
    corrida_iniciar(&corrida, dip_tipo_nav(hal_dip_ler()));

    uint32_t parado_desde = hal_tempo_ms();
    bool ajustado = false; /* já se aproximou da parede da frente nesta parada */

    /* Executa o que a lib pediu depois de um evento. */
    auto aplicar = [&](const corrida_efeitos_t &ef) {
        if (ef.parar_motores) {
            exec_parar();
        }
        if (ef.tem_falha) {
            publicar_falha_pronta(&ef.falha);
        }
#ifdef HAL_SIMULADA
        if (ef.reposicionar) {
            acompanhar_celula(&corrida.nav);
        }
#endif
        if (ef.bipes == 1) {
            Serial.println("# BOOT curto: mapa apagado, tipos resolvidos mantidos");
        } else if (ef.bipes == 2) {
            Serial.println("# BOOT longo: mapa e tipos resolvidos apagados");
        }
        if (ef.bipes > 0) {
            bipe(ef.bipes);
        }
        if (ef.parar_motores || ef.reposicionar) {
            parado_desde = hal_tempo_ms();
        }
    };

    auto health_check = [&]() {
        exec_parar();
        bool aprovado = publicar_health_check(corrida.inicio, &bateria);
        corrida_efeitos_t ef;
        corrida_health_check_concluido(&corrida, aprovado, dip_tipo_nav(hal_dip_ler()), &ef);
        aplicar(ef);
    };

    auto evento = [&](maq_evento_t ev) {
        corrida_efeitos_t ef;
        bool andava = maq_andando(&corrida.maq);
        corrida_evento(&corrida, ev, &ef);
        aplicar(ef);
        if (ev == MAQ_EV_CMD_LARGAR || ev == MAQ_EV_CMD_LARGAR_FORCADO) {
            parado_desde = hal_tempo_ms();
            /*
             * Largada: o operador acabou de pôr o robô no meio da célula, então a lateral e o rumo estimados
             * voltam a zero. Sem isto, depois de um sucesso a corrida seguinte partia com a estimativa da anterior
             * (pista, 25/11/2026: largou com lat=36.8 e rumo=-3.8° e foi raspando a parede esquerda).
             */
            if (!andava && maq_andando(&corrida.maq)) {
                exec_parar();
            }
        }
        if (ef.rodar_health_check) {
            health_check();
        }
    };

#ifdef HAL_SIMULADA
    acompanhar_celula(&corrida.nav);
#endif

    TickType_t proximo = xTaskGetTickCount();
    int ciclo = 0;
    int ciclos_amostra = 0;
#ifdef TELEMETRIA_DEPURACAO
    uint32_t volta_anterior = hal_tempo_ms();
#endif

    for (;;) {
        corrida_tempo(&corrida, hal_tempo_ms());
#ifdef TELEMETRIA_DEPURACAO
        uint32_t inicio_volta = hal_tempo_ms();
        if (inicio_volta - volta_anterior > app_nav_periodo_max_ms) {
            app_nav_periodo_max_ms = inicio_volta - volta_anterior;
        }
        volta_anterior = inicio_volta;
#endif
#ifdef HAL_SIMULADA
        /* O mundo simulado anda junto com o tempo real: sem PWM a pose não muda, com PWM ele anda. */
        sim_mundo_avancar(PERIODO_MS);
#endif

        /* Os ToF são lidos a 20 Hz para o filtro ter amostras novas. */
        if (++ciclo >= CICLOS_POR_LEITURA) {
            ciclo = 0;
            sensores_ler();
            informar_tof_ao_movimento();
#ifdef TELEMETRIA_DEPURACAO
            for (int i = 0; i < TOF_QTD; i++) {
                uint16_t mm = 0;
                if (sensores_distancia_mm((tof_id_t)i, &mm)) {
                    app_tof_ok = (uint8_t)(app_tof_ok | (1u << i));
                } else {
                    app_tof_ok = (uint8_t)(app_tof_ok & ~(1u << i));
                }
                app_tof_mm[i] = mm;
            }
#endif
        }

        /* Comandos e BOOT: a interrupção vem primeiro. */
        if (telemetria_consumir_interrupcao()) {
            evento(MAQ_EV_CMD_INTERROMPER);
        }
        botao_gesto_t gesto = botao_passo(&botao, hal_boot_pressionado(), hal_tempo_ms(),
                                          maq_andando(&corrida.maq));
        if (gesto != BOTAO_NADA) {
            evento(gesto == BOTAO_CURTO   ? MAQ_EV_BOOT_CURTO
                   : gesto == BOTAO_LONGO ? MAQ_EV_BOOT_LONGO
                                          : MAQ_EV_BOOT_TOQUE);
        }
        if (telemetria_consumir_health_check()) {
            evento(MAQ_EV_CMD_HEALTH_CHECK);
        }
        telemetria_largada_t largada = telemetria_consumir_largada_modo();
        if (largada != TELEMETRIA_LARGADA_NENHUMA) {
            evento(largada == TELEMETRIA_LARGADA_FORCADA ? MAQ_EV_CMD_LARGAR_FORCADO
                                                         : MAQ_EV_CMD_LARGAR);
        }

        /* Movimento: um ciclo do controle; a falha vira a falha automática stuck. */
        exec_estado_t exec = exec_atualizar();
        publicar_diagnostico_movimento();
        if (exec == EXEC_FALHOU) {
            corrida_efeitos_t ef;
            corrida_movimento_falhou(&corrida, &ef);
            exec_parar();
            aplicar(ef);
            exec = EXEC_OCIOSO;
        }

        if (exec != EXEC_OCIOSO) {
            parado_desde = hal_tempo_ms();
        } else if (maq_andando(&corrida.maq) && hal_tempo_ms() - parado_desde >= ESPERA_PARADO_MS && !ajustado &&
                   aproximar_da_parede()) {
            /* Parou antes do repouso: primeiro chega perto da parede da frente; a decisão vem depois. */
            ajustado = true;
            parado_desde = hal_tempo_ms();
        } else if (maq_andando(&corrida.maq) && hal_tempo_ms() - parado_desde >= ESPERA_PARADO_MS) {
            ajustado = false;
            /*
             * Parado na célula, com os ToF estabilizados: as três paredes são as lidas agora. Com os laterais a 90°,
             * o sensor parado olha a própria célula; a leitura em movimento errava na troca de célula (24/11/2026).
             */
            paredes_t paredes = sensores_paredes();
            nav_leitura_t leitura;
            leitura.frente = paredes.frente;
            leitura.esquerda = paredes.esquerda;
            leitura.direita = paredes.direita;

            bool reposicionar = false;
            telemetria_decisao_evento_t saida[TELEMETRIA_DECISAO_EVENTOS_MAX];
            int qtd = corrida_decidir(&corrida, leitura, &reposicionar, saida);
            for (int i = 0; i < qtd; i++) {
                switch (saida[i].tipo) {
                case TELEMETRIA_DECISAO_PASSO:
                    publicar_passo((int8_t)saida[i].x, (int8_t)saida[i].y, saida[i].paredes);
                    break;
                case TELEMETRIA_DECISAO_SUCESSO:
                    publicar_sucesso((int8_t)saida[i].x, (int8_t)saida[i].y);
                    break;
                case TELEMETRIA_DECISAO_SEM_SAIDA:
                    publicar_falha(CONTRATO_MOTIVO_STUCK, (int8_t)saida[i].x, (int8_t)saida[i].y,
                                   CONTRATO_COMPONENTE_NENHUM);
                    break;
                }
            }
            /*
             * O sensor manda: se a ação vai para um lado onde ele vê parede agora, o robô não anda para lá. A
             * navegação recebe o bloqueio e escolhe de novo; se insistir, a corrida falha parada, em vez de o
             * robô empurrar a parede (pista, 26/11/2026: "frente" com parede a 3 cm).
             */
            bool contra_parede = vai_contra_parede(corrida.acao, leitura);
            for (int i = 0; i < 4 && contra_parede; i++) {
                corrida.acao = nav_bloqueado(&corrida.nav);
                contra_parede = vai_contra_parede(corrida.acao, leitura);
            }
            publicar_diagnostico_decisao(&corrida.nav, leitura, corrida.acao, bateria_mv(&bateria));
            if (contra_parede) {
                corrida_efeitos_t ef;
                corrida_movimento_falhou(&corrida, &ef);
                exec_parar();
                aplicar(ef);
            } else if (exec_eh_movimento(corrida.acao)) {
                uint16_t frente_mm = 0;
                sensores_distancia_mm(TOF_FRONTAL, &frente_mm);
                exec_informar_frente_mm(frente_mm);
                exec_informar_frente(leitura.frente);
                exec_informar_lados(leitura.esquerda, leitura.direita);
                exec_comecar(corrida.acao);
            }
            parado_desde = hal_tempo_ms();
        }

        if (++ciclos_amostra >= CICLOS_POR_AMOSTRA) {
            ciclos_amostra = 0;
            bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
            if (bateria_pronta(&bateria)) {
                hal_motor_definir_tensao(bateria_ponte_mv(&bateria));
            }

            telemetria_amostra_t amostra = {};
            amostra.tel.estado = corrida_estado_contrato(&corrida);
            /*
             * A navegação já aponta a célula de destino quando devolve o movimento (D4). Em movimento, a tela mostra
             * a célula de onde o robô saiu; a de destino só depois de ele chegar.
             */
            bool em_movimento = exec_estado() == EXEC_GIRANDO || exec_estado() == EXEC_AVANCANDO;
            amostra.tel.x = (uint8_t)(em_movimento ? corrida.ultimo_x : corrida.nav.x);
            amostra.tel.y = (uint8_t)(em_movimento ? corrida.ultimo_y : corrida.nav.y);
            amostra.tel.rumo = telemetria_rumo_do_nav(corrida.nav.direcao);
            amostra.tel.bat_mv = bateria_mv(&bateria);
            amostra.tel.vel_mm_s = 0;
            amostra.tel.eixo_longo = CONTRATO_EIXO_DESCONHECIDO;
            amostra.em_movimento = corrida_em_movimento(&corrida);
            app_publicar_amostra(&amostra);
        }

        hal_led_definir(sinal_led(&sinal, corrida.maq.estado, hal_tempo_ms()));
        vTaskDelayUntil(&proximo, pdMS_TO_TICKS(PERIODO_MS));
    }
}
