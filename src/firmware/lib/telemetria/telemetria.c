#include "telemetria.h"

#include <stdatomic.h>

#include "hal_bt.h"
#include "hal_nvs.h"

static uint32_t mais_antigo(const telemetria_buffer_t *b)
{
    return b->total - b->qtd;
}

static telemetria_evento_t *evento_em(telemetria_buffer_t *b, uint32_t indice)
{
    return &b->eventos[indice % TELEMETRIA_BUFFER_EVENTOS];
}

void telemetria_buffer_iniciar(telemetria_buffer_t *b)
{
    b->total = 0;
    b->qtd = 0;
    b->cursor = 0;
    b->carimbados_ate = 0;
}

void telemetria_buffer_inserir(telemetria_buffer_t *b, const telemetria_evento_t *e)
{
    if (b->qtd == TELEMETRIA_BUFFER_EVENTOS) {
        /* O novo ocupa a posição do mais antigo, que sai do buffer. */
        b->qtd--;
        uint32_t antigo = mais_antigo(b);
        if (b->cursor < antigo) {
            b->cursor = antigo;
        }
        if (b->carimbados_ate < antigo) {
            b->carimbados_ate = antigo;
        }
    }
    *evento_em(b, b->total) = *e;
    b->total++;
    b->qtd++;
}

bool telemetria_buffer_pendente(const telemetria_buffer_t *b)
{
    return b->cursor < b->total;
}

int telemetria_buffer_serializar(telemetria_buffer_t *b, char *buf, size_t tam, uint32_t boot,
                                 uint32_t *seq, uint32_t agora_ms)
{
    if (!telemetria_buffer_pendente(b)) {
        return -1;
    }
    telemetria_evento_t *e = evento_em(b, b->cursor);
    if (b->cursor == b->carimbados_ate) {
        e->cab = (contrato_cabecalho_t){.boot = boot, .seq = *seq, .t_ms = agora_ms};
        (*seq)++;
        b->carimbados_ate++;
    }

    switch (e->tipo) {
    case TELEMETRIA_EVENTO_HC_ITEM:
        return contrato_hc_item(buf, tam, &e->cab, &e->m.hc_item);
    case TELEMETRIA_EVENTO_HC_RESULTADO:
        return contrato_hc_resultado(buf, tam, &e->cab, &e->m.hc_resultado);
    case TELEMETRIA_EVENTO_PASSO:
        return contrato_passo(buf, tam, &e->cab, &e->m.passo);
    case TELEMETRIA_EVENTO_FALHA:
        return contrato_falha(buf, tam, &e->cab, &e->m.falha);
    case TELEMETRIA_EVENTO_SUCESSO:
        return contrato_sucesso(buf, tam, &e->cab, &e->m.sucesso);
    }
    return -1;
}

void telemetria_buffer_avancar(telemetria_buffer_t *b)
{
    if (telemetria_buffer_pendente(b)) {
        b->cursor++;
    }
}

void telemetria_buffer_rebobinar(telemetria_buffer_t *b)
{
    b->cursor = mais_antigo(b);
}

/* Estado da telemetria: um só link, então um só estado. */
static telemetria_buffer_t buffer;
static uint32_t boot_atual;
static uint32_t seq;
static bool conectado_antes;
static bool tel_enviada, evento_enviado;
static uint32_t ultima_tel_ms, ultimo_evento_ms;

/* Linha em montagem: até TELEMETRIA_LINHA_RECEBIDA - 1 bytes antes do "\n". */
static char linha_recebida[TELEMETRIA_LINHA_RECEBIDA];
static size_t tamanho_recebido;
static bool descartando; /* linha longa: ignora até o próximo "\n" */

/*
 * Ligada pela tarefa de telemetria (núcleo 0) e consumida pela navegação
 * (núcleo 1). stdatomic em 32 bits: a ESP32 faz a troca com S32C1I, sem
 * trava, e ler e zerar numa só operação não perde um comando que chegue entre
 * as duas. Um volatile só garantiria a leitura, não a troca.
 */
static atomic_uint interrupcao;

void telemetria_iniciar(uint32_t boot)
{
    telemetria_buffer_iniciar(&buffer);
    boot_atual = boot;
    seq = 0;
    conectado_antes = false;
    tel_enviada = evento_enviado = false;
    ultima_tel_ms = ultimo_evento_ms = 0;
    tamanho_recebido = 0;
    descartando = false;
    atomic_store(&interrupcao, 0u);
}

void telemetria_inserir_evento(const telemetria_evento_t *e)
{
    telemetria_buffer_inserir(&buffer, e);
}

/* ms que faltam para o prazo (0 se já venceu); a subtração aguenta a volta do relógio. */
static uint32_t faltam(uint32_t agora_ms, bool houve_envio, uint32_t ultimo_ms, uint32_t periodo)
{
    uint32_t passou = agora_ms - ultimo_ms;
    return (!houve_envio || passou >= periodo) ? 0 : periodo - passou;
}

static uint32_t periodo_tel(const telemetria_amostra_t *amostra)
{
    return amostra->em_movimento ? TELEMETRIA_PERIODO_TEL_MOVIMENTO_MS
                                 : TELEMETRIA_PERIODO_TEL_PARADO_MS;
}

/* Escreve a linha inteira; menos bytes aceitos conta como queda do link. */
static bool escrever(const char *linha, int tamanho)
{
    if (tamanho <= 0) {
        return false;
    }
    if (hal_bt_escrever(linha, (size_t)tamanho) == (size_t)tamanho) {
        return true;
    }
    conectado_antes = false; /* a próxima volta vê a borda e rebobina */
    return false;
}

static void enviar_tel(uint32_t agora_ms, const telemetria_amostra_t *amostra)
{
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    contrato_cabecalho_t cab = {.boot = boot_atual, .seq = seq, .t_ms = agora_ms};
    if (escrever(linha, contrato_tel(linha, sizeof linha, &cab, &amostra->tel))) {
        seq++;
        tel_enviada = true;
        ultima_tel_ms = agora_ms;
    }
}

static void enviar_evento(uint32_t agora_ms)
{
    char linha[CONTRATO_TAMANHO_MAXIMO + 1];
    int tamanho = telemetria_buffer_serializar(&buffer, linha, sizeof linha, boot_atual, &seq,
                                               agora_ms);
    if (escrever(linha, tamanho)) {
        telemetria_buffer_avancar(&buffer);
        evento_enviado = true;
        ultimo_evento_ms = agora_ms;
    }
}

/* Monta as linhas que chegaram da ponte e confere cada uma. */
static void ler_comandos(void)
{
    int c;
    while ((c = hal_bt_ler_byte()) != -1) {
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (!descartando) {
                linha_recebida[tamanho_recebido] = '\0';
                if (contrato_eh_interromper(linha_recebida)) {
                    atomic_store(&interrupcao, 1u);
                }
            }
            tamanho_recebido = 0;
            descartando = false;
        } else if (tamanho_recebido < TELEMETRIA_LINHA_RECEBIDA - 1) {
            linha_recebida[tamanho_recebido++] = (char)c;
        } else {
            descartando = true;
        }
    }
}

bool telemetria_consumir_interrupcao(void)
{
    return atomic_exchange(&interrupcao, 0u) != 0u;
}

uint32_t telemetria_passo(uint32_t agora_ms, const telemetria_amostra_t *amostra)
{
    ler_comandos();

    bool conectado = hal_bt_conectado();
    if (conectado && !conectado_antes) {
        telemetria_buffer_rebobinar(&buffer);
    }
    conectado_antes = conectado;

    if (conectado && amostra != NULL &&
        faltam(agora_ms, tel_enviada, ultima_tel_ms, periodo_tel(amostra)) == 0) {
        enviar_tel(agora_ms, amostra);
    }
    if (conectado_antes && telemetria_buffer_pendente(&buffer) &&
        faltam(agora_ms, evento_enviado, ultimo_evento_ms, TELEMETRIA_PERIODO_EVENTO_MS) == 0) {
        enviar_evento(agora_ms);
    }

    /* Desconectado (ou queda agora): só volta a conferir o link daqui a pouco. */
    uint32_t espera = TELEMETRIA_PERIODO_EVENTO_MS;
    if (!conectado_antes) {
        return espera;
    }
    if (amostra != NULL) {
        uint32_t f = faltam(agora_ms, tel_enviada, ultima_tel_ms, periodo_tel(amostra));
        espera = f < espera ? f : espera;
    }
    if (telemetria_buffer_pendente(&buffer)) {
        uint32_t f = faltam(agora_ms, evento_enviado, ultimo_evento_ms,
                            TELEMETRIA_PERIODO_EVENTO_MS);
        espera = f < espera ? f : espera;
    }
    return espera;
}

uint32_t telemetria_proximo_boot(void)
{
    uint32_t boot = 0;
    if (!hal_nvs_ler(TELEMETRIA_CHAVE_BOOT, &boot, sizeof(boot))) {
        boot = 0;
    }
    boot++;
    /* Sem gravar, o próximo reboot repete este boot; melhor que não ligar. */
    (void)hal_nvs_gravar(TELEMETRIA_CHAVE_BOOT, &boot, sizeof(boot));
    return boot;
}
