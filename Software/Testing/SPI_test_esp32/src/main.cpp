// ESP32-S3 - SPI MASTER diagnóstico (SCK=16, MOSI=5, MISO=6, CS=7)
#include <Arduino.h>
#include <SPI.h>

static constexpr int PIN_SCK  = 16;
static constexpr int PIN_MISO = 18;
static constexpr int PIN_MOSI = 5;
static constexpr int PIN_CS   = 7;

// Velocidades a barrer (podés agregar más)
SPISettings speeds[] = {
  SPISettings(100000,  MSBFIRST, SPI_MODE0),
  SPISettings(1000000, MSBFIRST, SPI_MODE0),
  SPISettings(2000000, MSBFIRST, SPI_MODE0),
};

bool do_one_transfer(const SPISettings& sett) {
  uint8_t tx[16], rx[16];
  for (int i=0;i<16;i++) tx[i]=i;

  SPI.beginTransaction(sett);
  digitalWrite(PIN_CS, LOW);

  uint8_t seed = SPI.transfer(0x00);  // leer semilla del slave
  for (int i=0;i<16;i++) rx[i] = SPI.transfer(tx[i]);

  digitalWrite(PIN_CS, HIGH);
  SPI.endTransaction();

  // Mostrar
  Serial.printf("[S3] clk=%lu Hz seed=%02X | rx:", sett._clock, seed);
  for (int i=0;i<16;i++) Serial.printf(" %02X", rx[i]);
  Serial.println();

  // Clasificación
  auto all_equal_to = [&](uint8_t v){
    for (int i=0;i<16;i++) if (rx[i]!=v) return false;
    return true;
  };

  bool eco_ok = true;
  for (int i=0;i<16;i++) if (rx[i] != (uint8_t)(tx[i]^0xA5)) eco_ok=false;

  if (seed==0x5A && eco_ok) {
    Serial.println("[S3][OK] Semilla correcta y ECO ^0xA5 correcto. Todo OK ✅");
    return true;
  }

  if (all_equal_to(0x00)) {
    Serial.println("[S3][FAIL] RX todo 0x00 -> MISO a GND / sin conectar / slave no conduce / CS no en función SPI en RP.");
    return false;
  }

  if (all_equal_to(0xFF)) {
    Serial.println("[S3][FAIL] RX todo 0xFF -> MISO con pull-up / sin conducción desde el RP2040.");
    return false;
  }

  if (seed!=0x5A) {
    Serial.printf("[S3][WARN] Semilla inesperada: %02X (esperado 5A). Revise que el RP precargue 0x5A y que CS/SCK lleguen.\n", seed);
  }

  if (!eco_ok) {
    Serial.println("[S3][WARN] ECO incorrecto. Pistas cruzadas MOSI/MISO o CPOL/CPHA mal. Estamos en MODE0; verifique el RP.");
  }

  return false;
}

void setup(){
  Serial.begin(115200);
  delay(50);
  Serial.println("\n[S3] === ESP32-S3 - Diagnóstico SPI MASTER ===");
  pinMode(PIN_CS, OUTPUT); digitalWrite(PIN_CS, HIGH);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  Serial.printf("[S3] Pins: SCK=%d MOSI=%d MISO=%d CS=%d\n", PIN_SCK, PIN_MOSI, PIN_MISO, PIN_CS);

  // Opcional: test rápido de MISO como entrada
  pinMode(PIN_MISO, INPUT);
  int v = digitalRead(PIN_MISO);
  Serial.printf("[S3] Lectura digital inicial de MISO=%d (solo informativa)\n", v);
}

void loop(){
  bool any_ok = false;
  for (auto& sett : speeds) {
    bool ok = do_one_transfer(sett);
    any_ok = any_ok || ok;
    delay(300);
  }
  if (!any_ok) {
    Serial.println("[S3] Resultado: sin eco válido. Recomendaciones:");
    Serial.println(" - Verificar continuidad RP GPIO12 <-> S3 MISO (ohmímetro).");
    Serial.println(" - Confirmar que en RP el CS está en función SPI (gpio_set_function(PIN_CS, GPIO_FUNC_SPI)).");
    Serial.println(" - Asegurar GND común y que SCK llegue al RP GPIO10 (preflight RP).");
    Serial.println(" - Probar invertir MODE (SPI_MODE1/2/3) si hardware ajeno.");
  }
  Serial.println("---- Ciclo de diagnóstico completo ----\n");
  delay(1500);
}