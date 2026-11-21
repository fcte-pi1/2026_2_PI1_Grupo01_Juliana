/*
 * CONTROLE MANUAL - Ratatouille PI1 2026.2
 * Programa a parte (bancada), nao e o firmware do robo.
 *
 * Comandos (pelo monitor serial OU pelo celular via Bluetooth):
 *   w = frente      x = re
 *   a = gira para a esquerda (no lugar)
 *   d = gira para a direita  (no lugar)
 *   espaco = PARAR    y = buzina
 *   + = mais rapido   - = mais devagar
 *
 * App do celular (setas/joystick), maiusculas ou minusculas:
 *   f=frente b=re l=esquerda r=direita s=parar  y=buzina
 *   G/I = frente-esquerda/frente-direita, H/J = re-esquerda/re-direita
 *   0..9 e q = velocidade (q = maxima)
 *
 * O robo continua no ultimo comando ate receber outro.
 * SEGURANCA: para ao soltar o botao do app (ele manda 'S') e tambem
 * se o celular desconectar do Bluetooth.
 *
 * Bluetooth: no celular, pareie com "Ratatouille" e use um app de
 * terminal Bluetooth (ex.: "Serial Bluetooth Terminal", Android).
 *
 * Ponte H TB6612: PWMA 25, AIN1 26, AIN2 15 | PWMB 27, BIN1 14, BIN2 18 | STBY 13
 */
#include <Arduino.h>
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

// Se um motor girar ao contrario do esperado, troque o 1 por -1 dele
#define SENTIDO_E  1
#define SENTIDO_D  1

#define VEL_MIN    60
#define VEL_MAX    180      // limite seguro com a bateria 2S
#define PASSO_VEL  20
#define DEBUG_BT   1        // 1 = mostra no monitor cada letra que chega do celular
#define TIMEOUT_MS 0        // 0 = desligado (o app manda 'S' ao soltar o botao)
#define IGNORA_S_MS 300     // ignora 'S' que chega logo depois de um comando de movimento

BluetoothSerial BT;
int vel = 120;
char estado = 'x';
uint32_t t_ultimo = 0;
uint32_t t_mov = 0;      // quando chegou o ultimo comando de movimento

// buzina: onda quadrada de 2 kHz por 'ms' (funciona com buzzer ativo ou passivo)
void buzina(int ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < (uint32_t)ms) {
    digitalWrite(BUZZER, HIGH); delayMicroseconds(250);
    digitalWrite(BUZZER, LOW);  delayMicroseconds(250);
  }
}

void motor(int pwm, int in1, int in2, int v) {
  v = constrain(v, -VEL_MAX, VEL_MAX);
  digitalWrite(in1, v > 0 ? HIGH : LOW);
  digitalWrite(in2, v < 0 ? HIGH : LOW);
  analogWrite(pwm, abs(v));
}

void mover(int ve, int vd) {
  motor(PWMA, AIN1, AIN2, ve * SENTIDO_E);
  motor(PWMB, BIN1, BIN2, vd * SENTIDO_D);
}

void aplicar() {
  switch (estado) {
    case 'w': mover(vel, vel);   break;   // frente
    case 's': mover(-vel, -vel); break;   // re
    case 'a': mover(-vel, vel);  break;   // gira p/ esquerda
    case 'd': mover(vel, -vel);  break;   // gira p/ direita
    case 'q': mover(vel / 3, vel);   break;   // frente-esquerda (curva)
    case 'e': mover(vel, vel / 3);   break;   // frente-direita (curva)
    case 'z': mover(-vel / 3, -vel); break;   // re-esquerda
    case 'c': mover(-vel, -vel / 3); break;   // re-direita
    default:  mover(0, 0);       break;   // parado
  }
}

const char *nome(char e) {
  switch (e) {
    case 'w': return "FRENTE";
    case 's': return "RE";
    case 'a': return "ESQUERDA";
    case 'd': return "DIREITA";
    case 'q': return "FRENTE-ESQ";
    case 'e': return "FRENTE-DIR";
    case 'z': return "RE-ESQ";
    case 'c': return "RE-DIR";
    default:  return "PARADO";
  }
}

void avisar(const String &msg) {
  Serial.println(msg);
  if (BT.hasClient()) BT.println(msg);
}

void tratar(char c) {
  if (c == '\r' || c == '\n') return;
  char anterior = estado; int vel_ant = vel;
  t_ultimo = millis();
  switch (c) {
    // app do celular (f/b/l/r/s, maiusculas ou minusculas)
    case 'f': case 'F': estado = 'w'; break;
    case 'b': case 'B': estado = 's'; break;
    case 'l': case 'L': estado = 'a'; break;
    case 'r': case 'R': estado = 'd'; break;
    case 's': case 'S':
      if (estado != 'x' && millis() - t_mov < IGNORA_S_MS) return;   // 'S' falso logo apos o comando
      estado = 'x'; break;
    case 'y': case 'Y': buzina(200); return;
    case 'G': estado = 'q'; break;   // frente-esquerda
    case 'I': estado = 'e'; break;   // frente-direita
    case 'H': estado = 'z'; break;   // re-esquerda
    case 'J': estado = 'c'; break;   // re-direita
    // teclado do notebook
    case 'w': estado = 'w'; break;
    case 'x': estado = 's'; break;
    case 'a': estado = 'a'; break;
    case 'd': estado = 'd'; break;
    case ' ': estado = 'x'; break;
    case '+': vel = min(vel + PASSO_VEL, VEL_MAX); break;
    case '-': vel = max(vel - PASSO_VEL, VEL_MIN); break;
    case 'q': vel = VEL_MAX; break;
    default:
      if (c >= '0' && c <= '9') vel = map(c - '0', 0, 9, VEL_MIN, VEL_MAX);
      else return;
  }
  if (estado != 'x') t_mov = millis();
  aplicar();
  // so avisa quando muda (o app RC manda comandos o tempo todo)
  if (estado != anterior || vel != vel_ant)
    avisar(String(nome(estado)) + "  (vel " + vel + ")");
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

  BT.begin("Ratatouille");
  delay(300);
  Serial.println("\n===== CONTROLE MANUAL - Ratatouille =====");
  Serial.println("Teclado: w=frente  x=re  a=esquerda  d=direita  espaco=parar  +/-=velocidade");
  Serial.println("App: f=frente  b=re  l=esquerda  r=direita  s=parar  y=buzina");
  Serial.println("Bluetooth: \"Ratatouille\"");
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

  // seguranca 1: celular desconectou -> para
  static bool tinha_cliente = false;
  bool tem_cliente = BT.hasClient();
  if (tinha_cliente && !tem_cliente && estado != 'x') {
    estado = 'x';
    aplicar();
    Serial.println("Bluetooth desconectou - PARADO");
  }
  tinha_cliente = tem_cliente;

  // seguranca 2 (opcional): sem comando por TIMEOUT_MS, para
  if (TIMEOUT_MS > 0 && estado != 'x' && millis() - t_ultimo > TIMEOUT_MS) {
    estado = 'x';
    aplicar();
    avisar("Sem comando - PARADO");
  }

  digitalWrite(LED, estado != 'x');   // LED aceso = motores ligados
  delay(5);
}
