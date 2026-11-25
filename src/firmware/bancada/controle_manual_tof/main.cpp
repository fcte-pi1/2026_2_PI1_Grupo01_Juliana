/*
 * CONTROLE MANUAL COM PROTECAO NAS PAREDES - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Os 3 ToF protegem o robo indo para frente:
 *   FRONTAL : perto da parede a velocidade cai; no limite ele freia, apita
 *             e recusa ir para frente (re e giro continuam liberados).
 *   LATERAIS: (olham 45 graus para frente) perto de uma parede lateral ele
 *             desvia sozinho para o outro lado; muito perto, freia e bloqueia.
 *             Tambem NAO deixa girar para o lado que tem obstaculo perto.
 *   A re NAO tem sensor: cuidado ao dar re.
 *
 * Giro (a/d, l/r no app): mais lento que a frente e, num clique rapido,
 * gira so GIRO_CLIQUE_MS (antes eram 300 ms na velocidade cheia).
 *
 * Velocidade:  z = alterna LENTO / RAPIDO (comeca no LENTO)
 *
 * Comandos (monitor serial OU celular via Bluetooth):
 *   Teclado: w=frente x=re a=esquerda d=direita espaco=parar  z=lento/rapido
 *            y=buzina  v=mostra as distancias  m=liga/desliga as distancias
 *            o tempo todo  p=liga/desliga a protecao (CUIDADO)
 *   App:     f/b/l/r/s, y=buzina, G/I/H/J=diagonais, Z=lento/rapido
 *
 * Clique rapido no app: o 'S' que chega logo depois do movimento nao e
 * jogado fora; o robo anda ate completar IGNORA_S_MS e entao para.
 *
 * ToF (VL53L0X, I2C SDA 21 / SCL 22): ligados um por vez pelo XSHUT e
 * movidos para 0x30 (frente, XSHUT 33), 0x31 (esquerdo, 4), 0x32 (direito, 5).
 * Ponte H TB6612: PWMA 25, AIN1 26, AIN2 15 | PWMB 27, BIN1 14, BIN2 18 | STBY 13
 */
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>
#include "BluetoothSerial.h"

#define PWMA 25
#define AIN1 26
#define AIN2 15
#define PWMB 27
#define BIN1 14
#define BIN2 18
#define STBY 13
#define LED  2
#define BUZZER 12
#define SDA_PIN 21
#define SCL_PIN 22

#define SENTIDO_E  -1   // motor esquerdo montado invertido (igual ao firmware da Amanda)
#define SENTIDO_D  1

// ---- velocidade (PWM 0..255) ----
#define VEL_LENTA   75
#define VEL_RAPIDA  130
#define VEL_MIN     55      // abaixo disso o motor nao vence o atrito
#define VEL_GIRO_LENTO  65  // giro no lugar (a/d): mais lento que a frente
#define VEL_GIRO_RAPIDO 85
#define DEBUG_BT    1
#define IGNORA_S_MS 300     // clique rapido para frente/re: anda pelo menos isso
#define GIRO_CLIQUE_MS 150  // clique rapido no giro: gira so isso (menos = vira menos)

// ---- protecao (ajuste aqui, medindo com 'v' ou 'm' no labirinto) ----
// valores medidos com o dedo (21/11): encostado o frontal marca ~30, o esquerdo ~28
// e o direito ~65 (o direito le uns 35 mm a mais), por isso cada lado tem o seu limite
#define DIST_PARA_MM   100   // frontal: para a (100 + vel/4) mm -> lento ~118 mm, rapido ~132 mm
#define ZONA_LENTA_MM  150   // frontal: antes do limite a velocidade vai caindo
#define LAT_PARA_ESQ   95    // esquerdo: abaixo disso freia, nao anda para frente e nao gira para a esquerda
#define LAT_PARA_DIR   105   // direito:  idem para a direita
#define LAT_DESVIA_MAIS 40   // lateral: comeca a desviar a (limite + 40) mm
#define HISTERESE_MM   25    // para voltar a andar, precisa dessa folga a mais
#define SEM_LEITURA_MS 200   // frontal sem leitura nova -> para (sensor travou)
#define LONGE_MM       2000  // acima disso (ou 8190) conta como livre

enum { FRENTE = 0, ESQ = 1, DIR = 2 };
const char *NOME_TOF[3] = { "frente", "esquerdo", "direito" };
const int   XSHUT[3]    = { 33, 4, 5 };
const uint8_t ENDERECO[3] = { 0x30, 0x31, 0x32 };

VL53L0X tof[3];
bool tof_ok[3] = { false, false, false };
int dist[3] = { 9999, 9999, 9999 };
uint32_t t_leitura[3] = { 0, 0, 0 };

BluetoothSerial BT;
bool protecao = true;
bool mostrar = false;        // 'm': imprime as distancias o tempo todo

bool rapido = false;
int vel = VEL_LENTA;
char estado = 'x';
uint32_t t_mov = 0;
uint32_t parar_em = 0;
uint32_t t_aviso = 0;

void avisar(const String &msg) {
  Serial.println(msg);
  if (BT.hasClient()) BT.println(msg);
}

String mm(int i) {
  if (!tof_ok[i]) return "FALHOU";
  if (dist[i] >= 9999) return "livre";
  return String(dist[i]) + " mm";
}

void mostrar_distancias() {
  avisar("Frente " + mm(FRENTE) + " | Esq " + mm(ESQ) + " | Dir " + mm(DIR) +
         " | " + (rapido ? "RAPIDO" : "LENTO"));
}

int limite_frente() { return DIST_PARA_MM + vel / 4; }
bool indo_frente(char e) { return e == 'w' || e == 'q' || e == 'e'; }
int lat_para(int lado)   { return lado == ESQ ? LAT_PARA_ESQ : LAT_PARA_DIR; }
int lat_desvia(int lado) { return lat_para(lado) + LAT_DESVIA_MAIS; }
bool virando_esq(char e)  { return e == 'a' || e == 'q'; }
bool virando_dir(char e)  { return e == 'd' || e == 'e'; }
int vel_giro() { return rapido ? VEL_GIRO_RAPIDO : VEL_GIRO_LENTO; }

void motor(int pwm, int in1, int in2, int v) {
  v = constrain(v, -255, 255);
  digitalWrite(in1, v > 0 ? HIGH : LOW);
  digitalWrite(in2, v < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(v));
}

void mover(int ve, int vd) {
  motor(PWMA, AIN1, AIN2, ve * SENTIDO_E);
  motor(PWMB, BIN1, BIN2, vd * SENTIDO_D);
}

// freio curto do TB6612 (IN1 = IN2 = HIGH): para mais rapido que so cortar o PWM
void frear() {
  digitalWrite(AIN1, HIGH); digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, HIGH); digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, 0); analogWrite(PWMB, 0);
}

// velocidade para frente, reduzida perto da parede da frente
int vel_frente() {
  if (!protecao || !tof_ok[FRENTE]) return vel;
  int folga = dist[FRENTE] - limite_frente();
  if (folga >= ZONA_LENTA_MM) return vel;
  return constrain((int)map(folga, 0, ZONA_LENTA_MM, VEL_MIN, vel), VEL_MIN, vel);
}

// fator (0.3 .. 1.0) para a roda do lado OPOSTO a uma parede lateral perto:
// parede a esquerda -> roda direita mais lenta -> o robo vira para a direita
float fator_desvio(int lado) {
  if (!protecao || !tof_ok[lado] || dist[lado] >= lat_desvia(lado)) return 1.0f;
  float f = (float)(dist[lado] - lat_para(lado)) / LAT_DESVIA_MAIS;
  return 0.3f + 0.7f * constrain(f, 0.0f, 1.0f);
}

void aplicar() {
  int vf = vel_frente();
  float fe = fator_desvio(DIR);   // parede a direita -> roda esquerda mais lenta
  float fd = fator_desvio(ESQ);   // parede a esquerda -> roda direita mais lenta
  switch (estado) {
    case 'w': mover(vf * fe, vf * fd);           break;   // frente
    case 's': mover(-vel, -vel);                 break;   // re
    case 'a': mover(-vel_giro(), vel_giro());    break;   // gira p/ esquerda
    case 'd': mover(vel_giro(), -vel_giro());    break;   // gira p/ direita
    case 'q': mover(vf / 3 * fe, vf * fd);       break;   // frente-esquerda
    case 'e': mover(vf * fe, vf / 3 * fd);       break;   // frente-direita
    case 'z': mover(-vel / 3, -vel);             break;   // re-esquerda
    case 'c': mover(-vel, -vel / 3);             break;   // re-direita
    default:  mover(0, 0);                       break;
  }
}

const char *nome(char e) {
  switch (e) {
    case 'w': return "FRENTE";     case 's': return "RE";
    case 'a': return "ESQUERDA";   case 'd': return "DIREITA";
    case 'q': return "FRENTE-ESQ"; case 'e': return "FRENTE-DIR";
    case 'z': return "RE-ESQ";     case 'c': return "RE-DIR";
    default:  return "PARADO";
  }
}

// le os ToF sem travar o loop (so pega quando ha medida pronta)
void ler_tofs() {
  for (int i = 0; i < 3; i++) {
    if (!tof_ok[i]) continue;
    if ((tof[i].readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) == 0) continue;
    uint16_t v = tof[i].readReg16Bit(VL53L0X::RESULT_RANGE_STATUS + 10);
    tof[i].writeReg(VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
    dist[i] = (v == 0 || v >= LONGE_MM) ? 9999 : v;
    t_leitura[i] = millis();
  }
  // lateral travado: considera livre (o frontal travado para o robo, ver abaixo)
  for (int i = ESQ; i <= DIR; i++)
    if (tof_ok[i] && millis() - t_leitura[i] > SEM_LEITURA_MS) dist[i] = 9999;
}

// motivo para NAO fazer o movimento 'e', ou "" se pode.
// margem = HISTERESE_MM ao comecar um movimento, 0 enquanto ja esta andando
String perigo(char e, int margem) {
  if (!protecao) return "";
  if (indo_frente(e)) {
    if (!tof_ok[FRENTE]) return "sem sensor frontal";
    if (millis() - t_leitura[FRENTE] > SEM_LEITURA_MS) return "sensor frontal sem leitura";
    if (dist[FRENTE] <= limite_frente() + margem) return "parede na FRENTE a " + String(dist[FRENTE]) + " mm";
    if (tof_ok[ESQ] && dist[ESQ] <= lat_para(ESQ) + margem) return "parede na ESQUERDA a " + String(dist[ESQ]) + " mm";
    if (tof_ok[DIR] && dist[DIR] <= lat_para(DIR) + margem) return "parede na DIREITA a " + String(dist[DIR]) + " mm";
  }
  if (virando_esq(e) && tof_ok[ESQ] && dist[ESQ] <= lat_para(ESQ) + margem)
    return "obstaculo na ESQUERDA a " + String(dist[ESQ]) + " mm, nao giro para la";
  if (virando_dir(e) && tof_ok[DIR] && dist[DIR] <= lat_para(DIR) + margem)
    return "obstaculo na DIREITA a " + String(dist[DIR]) + " mm, nao giro para la";
  return "";
}

void apito(int ms);

// confere as paredes; chamado a cada volta do loop (e durante a buzina)
void protecao_paredes() {
  ler_tofs();
  if (estado == 'x') return;
  String motivo = perigo(estado, 0);
  if (motivo.length() > 0) {
    frear();
    estado = 'x';
    parar_em = 0;
    avisar(motivo + " - PARADO");
    apito(80);
  } else if (indo_frente(estado)) {
    aplicar();   // reaplica: reducao e desvio acompanham as distancias
  }
}

// onda de 2 kHz no buzzer (com transistor); continua vigiando as paredes
void apito(int ms) {
  uint32_t t0 = millis();
  int n = 0;
  while (millis() - t0 < (uint32_t)ms) {
    digitalWrite(BUZZER, HIGH); delayMicroseconds(250);
    digitalWrite(BUZZER, LOW);  delayMicroseconds(250);
    if (++n % 40 == 0 && estado != 'x') protecao_paredes();   // a cada ~20 ms
  }
}

void tratar(char c) {
  if (c == '\r' || c == '\n') return;
  char anterior = estado;
  switch (c) {
    case 'f': case 'F': case 'w': estado = 'w'; break;
    case 'b': case 'B': case 'x': estado = 's'; break;
    case 'l': case 'L': case 'a': estado = 'a'; break;
    case 'r': case 'R': case 'd': estado = 'd'; break;
    case 'G': estado = 'q'; break;
    case 'I': estado = 'e'; break;
    case 'H': estado = 'z'; break;
    case 'J': estado = 'c'; break;
    case 's': case 'S':
      {
        uint32_t minimo = (estado == 'a' || estado == 'd') ? GIRO_CLIQUE_MS : IGNORA_S_MS;
        if (estado != 'x' && millis() - t_mov < minimo) {   // clique rapido: para um pouco depois
          parar_em = t_mov + minimo;
          return;
        }
      }
      estado = 'x'; break;
    case ' ': estado = 'x'; break;
    case 'z': case 'Z':
      rapido = !rapido;
      vel = rapido ? VEL_RAPIDA : VEL_LENTA;
      aplicar();
      avisar(rapido ? "Velocidade: RAPIDO" : "Velocidade: LENTO");
      apito(rapido ? 120 : 40);
      return;
    case 'y': case 'Y': apito(200); return;
    case 'v': case 'V': mostrar_distancias(); return;
    case 'm': case 'M':
      mostrar = !mostrar;
      avisar(mostrar ? "Distancias: LIGADO (m desliga)" : "Distancias: DESLIGADO");
      return;
    case 'p': case 'P':
      protecao = !protecao;
      avisar(protecao ? "Protecao LIGADA" : "Protecao DESLIGADA - cuidado!");
      return;
    default: return;
  }

  // movimento perigoso: recusa (os outros continuam liberados)
  if (estado != anterior) {
    String motivo = perigo(estado, HISTERESE_MM);
    if (motivo.length() > 0) {
      estado = 'x';
      if (millis() - t_aviso > 500) {
        t_aviso = millis();
        avisar(motivo + ": movimento recusado");
      }
    }
  }

  bool cmd_mov = strchr("fFbBlLrRwxadGIHJ", c) != NULL;
  if (cmd_mov && estado != 'x') { t_mov = millis(); parar_em = 0; }   // novo movimento cancela a parada adiada
  if (estado == 'x') parar_em = 0;
  aplicar();
  if (estado != anterior) avisar(String(nome(estado)) + (rapido ? "  (rapido)" : "  (lento)"));
}

void iniciar_tofs() {
  for (int i = 0; i < 3; i++) { pinMode(XSHUT[i], OUTPUT); digitalWrite(XSHUT[i], LOW); }
  delay(10);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(50000);          // igual ao firmware da Amanda (cabos longos)
  for (int i = 0; i < 3; i++) {  // um por vez: liga, inicia e muda o endereco
    digitalWrite(XSHUT[i], HIGH);
    delay(10);
    tof[i].setTimeout(100);
    tof_ok[i] = tof[i].init();
    if (tof_ok[i]) {
      tof[i].setAddress(ENDERECO[i]);
      tof[i].setMeasurementTimingBudget(20000);   // ~20 ms por medida
      tof[i].startContinuous(0);
      t_leitura[i] = millis();
      Serial.printf("ToF %s OK (0x%02X)\n", NOME_TOF[i], ENDERECO[i]);
    } else {
      digitalWrite(XSHUT[i], LOW);   // desliga para nao atrapalhar os proximos
      Serial.printf("ToF %s NAO respondeu\n", NOME_TOF[i]);
    }
  }
  if (!tof_ok[FRENTE]) Serial.println("Sem o frontal a frente fica bloqueada (p desliga a protecao)");
  if (!tof_ok[ESQ] || !tof_ok[DIR]) Serial.println("Atencao: sem um lateral, aquele lado fica sem protecao");
}

void setup() {
  Serial.begin(115200);
  pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);
  mover(0, 0);
  digitalWrite(STBY, HIGH);

  iniciar_tofs();
  BT.begin("Ratatouille");
  delay(300);
  Serial.println("\n===== CONTROLE MANUAL + PROTECAO NAS PAREDES - Ratatouille =====");
  Serial.println("Teclado: w x a d, espaco=parar, z=lento/rapido, y=buzina, v/m=distancias, p=protecao");
  Serial.println("App: f b l r s, y=buzina, G I H J, Z=lento/rapido");
  mostrar_distancias();
}

void loop() {
  while (Serial.available()) tratar(Serial.read());
  while (BT.available()) {
    char c = BT.read();
#if DEBUG_BT
    if (c != '\n' && c != '\r') Serial.printf("[BT %6lu ms] '%c'\n", millis(), c);
#endif
    tratar(c);
  }

  // clique rapido: hora da parada adiada
  if (parar_em && (int32_t)(millis() - parar_em) >= 0) {
    parar_em = 0;
    estado = 'x';
    aplicar();
    avisar("PARADO");
  }

  protecao_paredes();

  // 'm': distancias a cada 300 ms
  static uint32_t t_mostra = 0;
  if (mostrar && millis() - t_mostra > 300) { t_mostra = millis(); mostrar_distancias(); }

  // celular desconectou -> para
  static bool tinha_cliente = false;
  bool tem_cliente = BT.hasClient();
  if (tinha_cliente && !tem_cliente && estado != 'x') {
    estado = 'x';
    aplicar();
    Serial.println("Bluetooth desconectou - PARADO");
  }
  tinha_cliente = tem_cliente;

  digitalWrite(LED, estado != 'x');
  delay(2);
}
