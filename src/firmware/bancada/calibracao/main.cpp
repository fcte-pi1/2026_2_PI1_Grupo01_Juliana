/*
 * Bancada de calibração no robô montado (HAL real): imprime o que o robô mediu para você comparar
 * com a régua, o transferidor e o multímetro. Não grava nada na NVS.
 *
 *   pio run -e esp32_calibracao -t upload
 *   pio device monitor
 *
 * Os motores só giram com o interruptor da bateria ligado. As teclas de movimento pedem robô no chão,
 * numa pista com espaço (o avanço de 10 células usa 1,8 m); a `v` e a `s` aceitam rodas no ar.
 *
 *   a   avança 1 célula              A   10 avanços seguidos (erro final até 10 mm)
 *   g   gira 90° à esquerda          G   gira 90° à direita          h   gira 180°
 *   Q   10 giros de 90° seguidos (desvio acumulado até 5°)
 *   v   velocidade: as duas rodas a 400 mm/s por 3 s (erro até 5%)
 *   s   stuck: gira o motor esquerdo e mede em quanto tempo a trava é detectada (segure a roda)
 *   t   ToF: 40 leituras com as paredes decididas (calibrar os limiares na pista)
 *   p   para os motores; uma linha vazia (só Enter) também para
 *
 * Cada comando é uma LINHA com uma tecla só: digite a tecla e dê Enter. Linhas com mais de uma letra são ignoradas.
 * Isso evita que o eco de um terminal (o /dev/rfcomm0 devolve o que recebe) vire comando: o robô imprime "a A g G ..."
 * e, sem esse cuidado, executaria o que ele mesmo escreveu.
 */

#include <Arduino.h>

#ifdef BT_REAL
#include <BluetoothSerial.h>
#include <esp_bt.h>
#endif

#include "bateria.h"
#include "config/robo.h"
#include "hal_bateria.h"
#include "hal_encoder.h"
#include "hal_motor.h"
#include "hal_tof.h"
#include "movimento.h"
#include "sensores.h"
#include "velocidade.h"

/*
 * Console: o USB e, com BT_REAL, também o Bluetooth (SPP "Ratatouille"). Tudo que sai vai para os dois,
 * e as teclas podem chegar por qualquer um, então dá para calibrar na pista sem cabo.
 */
#ifdef BT_REAL
/*
 * Avisos pelo USB. O callback roda na tarefa do Bluetooth, com pilha pequena: só guarda o evento,
 * e o loop imprime (avisar_bluetooth).
 */
static volatile int evento_pendente; /* 0 nada, 1 conectou, 2 caiu, 3 pareou, 4 pareamento falhou */
static volatile int status_pendente;

static void evento_spp(esp_spp_cb_event_t evento, esp_spp_cb_param_t *param)
{
    if (evento == ESP_SPP_SRV_OPEN_EVT) {
        evento_pendente = 1;
    } else if (evento == ESP_SPP_CLOSE_EVT) {
        status_pendente = (int)param->close.status;
        evento_pendente = 2;
    }
}

static void avisar_bluetooth(void)
{
    int e = evento_pendente;
    if (e == 0) {
        return;
    }
    evento_pendente = 0;
    if (e == 1) {
        Serial.println("# bt: cliente conectado");
    } else if (e == 2) {
        Serial.printf("# bt: desconectado (status %d)\n", status_pendente);
    } else if (e == 3) {
        Serial.println("# bt: pareamento concluido");
    } else {
        Serial.println("# bt: pareamento FALHOU");
    }
}
#else
static void avisar_bluetooth(void) {}
#endif

class Console : public Stream {
public:
    void iniciar()
    {
        Serial.begin(115200);
#ifdef BT_REAL
        bt_.register_callback(evento_spp);
        bt_.onAuthComplete([](boolean ok) { evento_pendente = ok ? 3 : 4; });
        bool ok = bt_.begin("Ratatouille");
        if (ok) {
            esp_bredr_tx_power_set(ESP_PWR_LVL_P9, ESP_PWR_LVL_P9);
        }
        Serial.printf("# bt: %s como \"Ratatouille\"\n", ok ? "aguardando conexao" : "NAO iniciou");
#endif
    }
    size_t write(uint8_t c) override
    {
        Serial.write(c);
#ifdef BT_REAL
        if (bt_.hasClient()) {
            bt_.write(c);
        }
#endif
        return 1;
    }
    size_t write(const uint8_t *dados, size_t n) override
    {
        Serial.write(dados, n);
#ifdef BT_REAL
        if (bt_.hasClient()) {
            bt_.write(dados, n);
        }
#endif
        return n;
    }
    int available() override
    {
        int n = Serial.available();
#ifdef BT_REAL
        n += bt_.available();
#endif
        return n;
    }
    int read() override
    {
        if (Serial.available()) {
            return Serial.read();
        }
#ifdef BT_REAL
        if (bt_.available()) {
            return bt_.read();
        }
#endif
        return -1;
    }
    int peek() override
    {
        if (Serial.available()) {
            return Serial.peek();
        }
#ifdef BT_REAL
        if (bt_.available()) {
            return bt_.peek();
        }
#endif
        return -1;
    }
    void flush() override {}

private:
#ifdef BT_REAL
    BluetoothSerial bt_;
#endif
};

static Console console;

#define PAUSA_ENTRE_MOVIMENTOS_MS 500
#define LIMITE_MOVIMENTO_MS       (MOV_TEMPO_LIMITE_MS + 500)

static bateria_t bateria;
static uint32_t ultima_bateria_ms;

/* Mantém a tensão da ponte H atualizada: sem ela o duty fica no valor da LiPo cheia. */
static void atualizar_bateria(void)
{
    uint32_t agora = millis();
    if (agora - ultima_bateria_ms < 200) {
        return;
    }
    ultima_bateria_ms = agora;
    bateria_amostrar(&bateria, hal_bateria_ler_adc_mv());
    if (bateria_pronta(&bateria)) {
        hal_motor_definir_tensao(bateria_ponte_mv(&bateria));
    }
}

static void parar_tudo(void)
{
    mov_parar();
    hal_motor_definir(MOTOR_ESQ, 0);
    hal_motor_definir(MOTOR_DIR, 0);
    hal_motor_habilitar(false);
}

/* Interrompe o que estiver em curso se chegar qualquer tecla. */
/*
 * Lê as linhas que chegaram. Devolve -1 se não há linha completa, -2 se a linha foi ignorada (mais de uma
 * letra, como o eco de uma saída do robô), 0 para a linha vazia e a tecla para uma linha de uma letra só.
 */
static int ler_comando(void)
{
    static char linha[24];
    static size_t tamanho;
    static bool estourou;
    static bool depois_de_cr;
    while (console.available()) {
        int c = console.read();
        if (c == '\n' && depois_de_cr) {
            depois_de_cr = false;
            continue;
        }
        depois_de_cr = c == '\r';
        if (c != '\r' && c != '\n') {
            if (tamanho < sizeof linha) {
                linha[tamanho++] = (char)c;
            } else {
                estourou = true;
            }
            continue;
        }
        size_t n = tamanho;
        char primeira = n > 0 ? linha[0] : 0;
        bool ignorar = estourou || n > 1;
        tamanho = 0;
        estourou = false;
        if (ignorar) {
            return -2;
        }
        return n == 0 ? 0 : primeira;
    }
    return -1;
}

/* Interrompe o que estiver em curso se chegar uma linha de uma letra só ou vazia. */
static bool tecla_de_parada(void)
{
    int c = ler_comando();
    if (c == -1 || c == -2) {
        return false;
    }
    parar_tudo();
    console.println("# parado por tecla");
    return true;
}

typedef struct {
    int32_t esq;
    int32_t dir;
    mov_estado_t estado;
} resultado_t;

static uint16_t frontal_mm;
static uint16_t historico_frontal[100];
static int historico_n;
static uint32_t leitura_max_us;

/* Lê os ToF e passa ao movimento, como a tarefa de navegação faz a cada 50 ms. */
static void informar_tof_ao_movimento(void)
{
    uint32_t t0 = micros();
    sensores_ler();
    uint32_t gasto = micros() - t0;
    if (gasto > leitura_max_us) {
        leitura_max_us = gasto;
    }
    paredes_t p = sensores_paredes();
    uint16_t frente = 0, esquerda = 0, direita = 0;
    sensores_distancia_mm(TOF_FRONTAL, &frente);
    sensores_distancia_mm(TOF_ESQUERDO, &esquerda);
    sensores_distancia_mm(TOF_DIREITO, &direita);
    frontal_mm = frente;
    if (historico_n < 100) {
        historico_frontal[historico_n++] = frente;
    }
    mov_tof_t tof = {p.frente, p.esquerda, p.direita, frente, esquerda, direita};
    mov_informar_tof(&tof);
}

/* O filtro mediano guarda as últimas leituras: encher com a posição de agora, senão a parada vê o robô onde ele estava. */
static void preparar_tof(void)
{
    for (int i = 0; i < 3 * TOF_MEDIANA_AMOSTRAS; i++) {
        sensores_ler();
        delay(50);
    }
}

/* Encoders a cada 50 ms durante o movimento: mostra se as rodas seguem a proporção pedida (arco). */
#define HISTORICO_ENC_MAX 80
static int32_t historico_esq[HISTORICO_ENC_MAX];
static int32_t historico_dir[HISTORICO_ENC_MAX];
static int historico_enc_n;

static void rodar_movimento(resultado_t *r)
{
    historico_enc_n = 0;
    int32_t esq0 = hal_encoder_ler(MOTOR_ESQ);
    int32_t dir0 = hal_encoder_ler(MOTOR_DIR);
    uint32_t inicio = millis();
    uint32_t proximo = micros();
    int ciclos_tof = 0;
    historico_n = 0;
    leitura_max_us = 0;
    while (mov_estado() == MOV_EM_CURSO && millis() - inicio < LIMITE_MOVIMENTO_MS) {
        if (tecla_de_parada()) {
            break;
        }
        atualizar_bateria();
        if (++ciclos_tof >= 10) {
            ciclos_tof = 0;
            informar_tof_ao_movimento();
            if (historico_enc_n < HISTORICO_ENC_MAX) {
                historico_esq[historico_enc_n] = hal_encoder_ler(MOTOR_ESQ) - esq0;
                historico_dir[historico_enc_n] = hal_encoder_ler(MOTOR_DIR) - dir0;
                historico_enc_n++;
            }
        }
        mov_atualizar();
        proximo += MOV_PERIODO_MS * 1000UL;
        while ((int32_t)(micros() - proximo) < 0) {
        }
    }
    r->estado = mov_estado();
    console.print("# frontal a cada 50 ms (0 = sem leitura):");
    for (int i = 0; i < historico_n; i++) {
        console.printf(" %u", (unsigned)historico_frontal[i]);
    }
    console.printf("\n# a leitura dos ToF levou no maximo %lu ms (o laco do movimento e de %d ms)\n", (unsigned long)(leitura_max_us / 1000), MOV_PERIODO_MS);
    r->esq = hal_encoder_ler(MOTOR_ESQ) - esq0;
    r->dir = hal_encoder_ler(MOTOR_DIR) - dir0;
}

static const char *nome_estado(mov_estado_t e)
{
    return e == MOV_CONCLUIDO ? "concluido" : e == MOV_FALHOU ? "FALHOU" : "interrompido";
}

static void imprimir_giro(const resultado_t &r, int graus)
{
    int32_t esperado = mov_pulsos_giro(graus < 0 ? -graus : graus);
    float girado = (r.dir - r.esq) / 2.0f / esperado * (graus < 0 ? -graus : graus);
    console.printf("giro: %s esq=%ld dir=%ld pulsos (esperado +-%ld) = %.1f graus (pedido %d)\n", nome_estado(r.estado),
                  (long)r.esq, (long)r.dir, (long)esperado, girado, graus);
}

static void imprimir(const char *quem, const resultado_t &r, int32_t esperado)
{
    float mm = (r.esq + r.dir) / 2.0f * MM_POR_PULSO;
    console.printf("%s: %s esq=%ld dir=%ld pulsos (esperado %ld) = %.1f mm pelos pulsos\n", quem,
                  nome_estado(r.estado), (long)r.esq, (long)r.dir, (long)esperado, mm);
}

static void avancar(void)
{
    resultado_t r;
    preparar_tof();
    uint16_t ao_partir = 0;
    sensores_distancia_mm(TOF_FRONTAL, &ao_partir);
    console.printf("# frontal ao partir: %u mm (a parada de emergencia deixa pelo menos %d mm ate a parede)\n", (unsigned)ao_partir,
                  (int)MOV_FRENTE_SOBRA_MM);
    mov_avancar_celula();
    rodar_movimento(&r);
    imprimir("avanco", r, mov_pulsos_avanco(CELULA_MM));
    console.printf("# frontal ao parar: %u mm (a parada garante pelo menos %d mm ate a parede)\n",
                  (unsigned)frontal_mm, (int)MOV_FRENTE_SOBRA_MM);
}

static void avancar_dez(void)
{
    long soma_esq = 0, soma_dir = 0;
    for (int i = 1; i <= 10; i++) {
        resultado_t r;
        preparar_tof();
        mov_avancar_celula();
        rodar_movimento(&r);
        console.printf("avanco %d: %s esq=%ld dir=%ld\n", i, nome_estado(r.estado), (long)r.esq,
                      (long)r.dir);
        soma_esq += r.esq;
        soma_dir += r.dir;
        if (r.estado != MOV_CONCLUIDO) {
            return;
        }
        delay(PAUSA_ENTRE_MOVIMENTOS_MS);
    }
    float mm = (soma_esq + soma_dir) / 2.0f * MM_POR_PULSO;
    console.printf("10 avancos: esq=%ld dir=%ld pulsos (esperado %ld) = %.1f mm pelos pulsos; "
                  "meça na pista: esperado %d mm, erro final de ate 10 mm\n",
                  soma_esq, soma_dir, (long)(10 * mov_pulsos_avanco(CELULA_MM)), mm, 10 * CELULA_MM);
}

static void girar(int graus)
{
    resultado_t r;
    mov_girar(graus);
    rodar_movimento(&r);
    imprimir_giro(r, graus);
}

/* Arco de 90° com o raio da corrida (GIRO_RAIO_ARCO_MM), sem a ré antes: o robô precisa de espaço livre. */
static void arco(int graus)
{
    resultado_t r;
    mov_arco((float)graus, GIRO_RAIO_ARCO_MM);
    rodar_movimento(&r);
    console.print("# pulsos esq/dir a cada 50 ms:");
    for (int i = 0; i < historico_enc_n; i++) {
        console.printf(" %ld/%ld", (long)historico_esq[i], (long)historico_dir[i]);
    }
    float girado = (r.dir - r.esq) * MM_POR_PULSO / BITOLA_MM * 180.0f / PI_F;
    console.printf("\narco: %s esq=%ld dir=%ld pulsos = %.1f graus pelos pulsos (pedido %d, raio %.0f mm)\n",
                  nome_estado(r.estado), (long)r.esq, (long)r.dir, girado, graus, (double)GIRO_RAIO_ARCO_MM);
}

static void girar_dez(void)
{
    long soma_esq = 0, soma_dir = 0;
    for (int i = 1; i <= 10; i++) {
        resultado_t r;
        mov_girar(90);
        rodar_movimento(&r);
        console.printf("giro %d: %s esq=%ld dir=%ld\n", i, nome_estado(r.estado), (long)r.esq,
                      (long)r.dir);
        soma_esq += r.esq;
        soma_dir += r.dir;
        if (r.estado != MOV_CONCLUIDO) {
            return;
        }
        delay(PAUSA_ENTRE_MOVIMENTOS_MS);
    }
    console.printf("10 giros: esq=%ld dir=%ld pulsos; o robô termina virado 180 graus em relação à partida (2,5 voltas); "
                  "meça o desvio com o transferidor: ate 5 graus\n",
                  soma_esq, soma_dir);
}

static void velocidade(void)
{
    const float alvo = VELOCIDADE_CRUZEIRO_MM_S;
    vel_iniciar();
    vel_definir_mm_s(MOTOR_ESQ, alvo);
    vel_definir_mm_s(MOTOR_DIR, alvo);
    double soma_esq = 0, soma_dir = 0;
    int amostras = 0;
    uint32_t inicio = millis();
    uint32_t ultima = 0;
    console.printf("velocidade: alvo %.0f mm/s nas duas rodas por 3 s\n", alvo);
    while (millis() - inicio < 3000) {
        if (tecla_de_parada()) {
            vel_definir_mm_s(MOTOR_ESQ, 0);
            vel_definir_mm_s(MOTOR_DIR, 0);
            return;
        }
        atualizar_bateria();
        vel_passo();
        uint32_t t = millis() - inicio;
        if (t - ultima >= 200) {
            ultima = t;
            float e = vel_mm_s(MOTOR_ESQ), d = vel_mm_s(MOTOR_DIR);
            console.printf("t=%lu ms esq=%.0f dir=%.0f mm/s\n", (unsigned long)t, e, d);
            if (t >= 1500) { /* depois da rampa */
                soma_esq += e;
                soma_dir += d;
                amostras++;
            }
        }
        delay(1);
    }
    vel_definir_mm_s(MOTOR_ESQ, 0);
    vel_definir_mm_s(MOTOR_DIR, 0);
    for (int i = 0; i < 100; i++) {
        vel_passo();
        delay(2);
    }
    parar_tudo();
    if (amostras > 0) {
        float media_e = (float)(soma_esq / amostras), media_d = (float)(soma_dir / amostras);
        float erro_e = (media_e - alvo) / alvo * 100.0f, erro_d = (media_d - alvo) / alvo * 100.0f;
        bool ok = fabsf(erro_e) <= 5.0f && fabsf(erro_d) <= 5.0f;
        console.printf("media depois da rampa: esq=%.0f (%.1f%%) dir=%.0f (%.1f%%) -> %s (limite 5%%)\n",
                      media_e, erro_e, media_d, erro_d, ok ? "APROVADO" : "REPROVADO");
    }
}

static void stuck(void)
{
    vel_iniciar();
    vel_definir_mm_s(MOTOR_ESQ, 300);
    uint32_t inicio = millis();
    console.println("stuck: motor esquerdo a 300 mm/s; segure a roda para travar (ate 8 s)");
    while (millis() - inicio < 8000 && !vel_travado()) {
        if (tecla_de_parada()) {
            return;
        }
        atualizar_bateria();
        vel_passo();
        delay(1);
    }
    uint32_t total = millis() - inicio;
    vel_definir_mm_s(MOTOR_ESQ, 0);
    for (int i = 0; i < 50; i++) {
        vel_passo();
        delay(2);
    }
    parar_tudo();
    if (vel_travado()) {
        console.printf("stuck detectado %lu ms depois do inicio (a trava vale %d ms de encoder parado)\n",
                      (unsigned long)total, MOTOR_TRAVADO_MS);
    } else {
        console.println("stuck: nao detectado em 8 s (a roda girou livre)");
    }
}

static void mostrar_tof(void)
{
    console.println("tof: frontal / esquerdo / direito em mm e paredes (F E D), 40 leituras a 20 Hz");
    for (int i = 0; i < 40; i++) {
        sensores_ler();
        uint16_t f = 0, e = 0, d = 0;
        bool tf = sensores_distancia_mm(TOF_FRONTAL, &f);
        bool te = sensores_distancia_mm(TOF_ESQUERDO, &e);
        bool td = sensores_distancia_mm(TOF_DIREITO, &d);
        paredes_t p = sensores_paredes();
        console.printf("%6s %6s %6s   F=%d E=%d D=%d\n", tf ? String(f).c_str() : "--",
                      te ? String(e).c_str() : "--", td ? String(d).c_str() : "--", p.frente,
                      p.esquerda, p.direita);
        delay(50);
    }
}

void setup()
{
    console.iniciar();
    mov_iniciar();
    hal_bateria_iniciar();
    bateria_iniciar(&bateria);
    bool tof_ok = hal_tof_iniciar();
    sensores_iniciar();
    console.println("bancada de calibracao: digite a tecla e Enter: a A g G h Q v s t p (Enter sozinho para)");
    console.printf("tof: %s\n", tof_ok ? "os 3 iniciaram" : "algum nao iniciou");
}

void loop()
{
    avisar_bluetooth();
    atualizar_bateria();
    int tecla = ler_comando();
    if (tecla == -1 || tecla == -2) {
        delay(5);
        return;
    }
    if (tecla == 0) {
        parar_tudo();
        return;
    }
    console.printf("# comando: %c\n", tecla);
    switch (tecla) {
    case 'a': avancar(); break;
    case 'A': avancar_dez(); break;
    case 'g': girar(90); break;
    case 'G': girar(-90); break;
    case 'h': girar(180); break;
    case 'Q': girar_dez(); break;
    case 'o': arco(90); break;
    case 'O': arco(-90); break;
    case 'v': velocidade(); break;
    case 's': stuck(); break;
    case 't': mostrar_tof(); break;
    default: parar_tudo(); break;
    }
}
