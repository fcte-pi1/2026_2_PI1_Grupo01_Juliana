/*
 * Teste rápido: rampa de PWM no TB6612FNG (canal A), código enviado pela equipe.
 * Programa à parte, não é o firmware do robô.
 * Atenção: estes pinos NÃO são os do include/config/pinos.h (lá o motor esquerdo
 * usa PWM 25, IN1 26, IN2 15 e STBY 13). D21/D22 são o I²C dos sensores ToF.
 */
#include <Arduino.h>

// Definição dos pinos (ajuste conforme a montagem de vocês)
const int AIN1 = 18;
const int AIN2 = 19;
const int PWMA = 21;
const int STBY = 22;

void setup() {
  Serial.begin(115200);

  // Configuração dos pinos como saída
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Tira a Ponte H do modo de economia de energia
  digitalWrite(STBY, HIGH);

  // Define o sentido de rotação (ex: frente)
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
}

void loop() {
  // TEMPORARIO (diagnostico): velocidade fixa em vez da rampa.
  // Para voltar a rampa original do colega, veja rampa_original.txt.
  analogWrite(PWMA, 200);   // velocidade fixa: 200 de 255 (~78%)
  Serial.println(200);
  delay(500);
}
