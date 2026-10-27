// Teste de Atuadores (Ponte H TB6612FNG e Motor N20)
// Dupla 2: Juan e Anderson - Equipe de Eletrônica / Hardware
// Micromouse Ratatuli - PI1 2026.2

#define PIN_PWMA  18  // Pino de controle de PWM (velocidade)
#define PIN_AIN1  19  // Pino de controle de sentido de rotação 1
#define PIN_AIN2  21  // Pino de controle de sentido de rotação 2
#define PIN_STBY  22  // Pino de Standby (HIGH para operação normal)

void setup() {
  Serial.begin(115200);

  // Configuração dos pinos como saídas
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_STBY, OUTPUT);

  // Habilita a ponte H retirando do modo standby
  digitalWrite(PIN_STBY, HIGH);

  // Define sentido de giro
  digitalWrite(PIN_AIN1, HIGH);
  digitalWrite(PIN_AIN2, LOW);
}

void loop() {
  // Rampa de PWM de 0 a 255 para aceleração progressiva
  for (int velocidade = 0; velocidade <= 255; velocidade++) {
    analogWrite(PIN_PWMA, velocidade);
    Serial.println(velocidade);
    delay(50);
  }
}
