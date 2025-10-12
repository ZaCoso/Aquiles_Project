/*
  Test básico para ESP32-S3
  - Parpadea el LED integrado (si lo tiene en GPIO 2)
  - Envía mensajes por el puerto serie cada segundo
*/

#define LED_PIN 2  // En muchas placas ESP32-S3 el LED está en el pin 2

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("✅ ESP32-S3 inicializado correctamente.");
  Serial.println("🔄 Comenzando parpadeo del LED...");

  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("💡 LED encendido");
  delay(1000);

  digitalWrite(LED_PIN, LOW);
  Serial.println("💤 LED apagado");
  delay(1000);
}