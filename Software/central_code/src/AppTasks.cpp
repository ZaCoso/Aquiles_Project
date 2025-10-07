#include "AppTasks.h"

///Semaphore
// xSemaphoreHandle semph_LeftMotor;
// xSemaphoreHandle semph_RightMotor;

/// Queues

NimBLECharacteristic *pCharacteristic;
bool deviceConnected = false;
std::string lastValue = "0";

void MyCallbacks::onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string value = std::string(pCharacteristic->getValue().c_str());

    if (value.length() > 0) {
        Serial.print("Recibido via BLE: ");
        Serial.println(value.c_str());

        // Por ejemplo, cambiar porcentaje de asistencia
        lastValue = value;
    }
}

void MyServerCallbacks::onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Dispositivo conectado");
}

void MyServerCallbacks::onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Dispositivo desconectado");
}

void initBLE() {


    // Inicializar BLE
    NimBLEDevice::init(DEVICE_NAME);

    // Crear servidor y callbacks
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Crear servicio
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Crear characteristic con propiedades estándar
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ   |
        NIMBLE_PROPERTY::WRITE  |
        NIMBLE_PROPERTY::NOTIFY
    );

    // Agregar descriptor CCCD (Client Characteristic Configuration Descriptor)
    // Esto habilita las notificaciones en la app cliente (Android/iOS)
    pCharacteristic->createDescriptor("2902");

    // Valor inicial y callback de escritura
    pCharacteristic->setValue("0");
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Iniciar el servicio
    pService->start();

    // Configurar y comenzar la publicidad
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    Serial.println("ESP32 BLE listo para conexiones");
}
void enviarEstado() {
    static int bat = 0;
    bat = (bat + 1) % 101;
    String mensaje = "BAT:" + String(bat);
    pCharacteristic->setValue(mensaje.c_str());
    pCharacteristic->notify();

    Serial.print("Enviado: ");
    Serial.println(mensaje);
}
void InitTask(void *params){
    // semph_LeftMotor = xSemaphoreCreateBinary();
    // semph_RightMotor = xSemaphoreCreateBinary();
    pinMode(46, OUTPUT);

    // pinMode(PIN_CS, OUTPUT);
    // digitalWrite(PIN_CS, HIGH);

    // SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    vTaskDelete(NULL);

}
void BlinkTask(void *params){
    while(1){
    digitalWrite(46, !digitalRead(46));

    Serial.printf("[SPI] Enviando: ");
    vTaskDelay(pdMS_TO_TICKS(500));}
}
// ---- Tarea que envía el paquete SPI cada 500 ms ----

// ====== Configuración por-dispositivo ======
// RP2040: 1 MHz, MSB, Mode0
static SPISettings RP2040_SETTINGS(1000000, MSBFIRST, SPI_MODE0);

// DEV_B: ejemplo 8 MHz, Mode3 (cámbialo según tu chip)
static SPISettings DEV_B_SETTINGS(8000000, MSBFIRST, SPI_MODE3);

// Mutex para el bus compartido
static SemaphoreHandle_t spiMutex = nullptr;

// ====== Helpers internos ======
static inline void busLock()   { xSemaphoreTake(spiMutex, portMAX_DELAY); }
static inline void busUnlock() { xSemaphoreGive(spiMutex); }

static inline uint8_t xfer(uint8_t b) { return SPI.transfer(b); }

// ====== CRC8 (polinomio 0x07, init 0x00) ======
uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init) {
  uint8_t crc = init;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t k = 0; k < 8; ++k)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

// ====== Init del BUS + CS ======
void spiBusInit() {
  // CS como salidas y en reposo (HIGH)
  pinMode(CS_RP2040, OUTPUT); digitalWrite(CS_RP2040, HIGH);
  pinMode(CS_DEV_B, OUTPUT);  digitalWrite(CS_DEV_B,  HIGH);

  // Iniciar el BUS con pines compartidos
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_RP2040);

  // Crear mutex
  if (!spiMutex) spiMutex = xSemaphoreCreateMutex();

  Serial.println("SPI Bus listo (un bus, multiples CS).");
}

// ====== Protocolo RP2040 (SOF/LEN/6 bytes/CRC) ======
bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                        uint8_t resp6[6], uint32_t timeout_ms)
{
  // Frame: [SOF=0xAA][LEN=6][v1_l][v1_h][v2_l][v2_h][v3_l][v3_h][CRC]
  uint8_t frame[9];
  frame[0] = 0xAA;
  frame[1] = 6;
  frame[2] = (uint8_t)(v1 & 0xFF);
  frame[3] = (uint8_t)((v1 >> 8) & 0xFF);
  frame[4] = (uint8_t)(v2 & 0xFF);
  frame[5] = (uint8_t)((v2 >> 8) & 0xFF);
  frame[6] = (uint8_t)(v3 & 0xFF);
  frame[7] = (uint8_t)((v3 >> 8) & 0xFF);
  frame[8] = crc8_sum(&frame[2], 6);

  busLock();
  SPI.beginTransaction(RP2040_SETTINGS);
  digitalWrite(CS_RP2040, LOW);

  // Enviar los 9 bytes (leer y descartar lo que venga durante el envío)
  for (size_t i = 0; i < sizeof(frame); ++i) (void)xfer(frame[i]);

  // Leer 6 bytes de respuesta (eco+1)
  const uint32_t t0 = millis();
  size_t got = 0;
  while (got < 6) {
    resp6[got++] = xfer(0x00); // dummy para clockear
    if ((millis() - t0) > timeout_ms) {
      digitalWrite(CS_RP2040, HIGH);
      SPI.endTransaction();
      busUnlock();
      return false; // timeout
    }
  }

  digitalWrite(CS_RP2040, HIGH);
  SPI.endTransaction();
  busUnlock();
  return true;
}

// ====== Transferencia genérica para DEV_B ======
bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n) {
  if (n == 0) return true;

  busLock();
  SPI.beginTransaction(DEV_B_SETTINGS);
  digitalWrite(CS_DEV_B, LOW);

  for (size_t i = 0; i < n; ++i) {
    uint8_t t = tx ? tx[i] : 0x00;
    uint8_t r = xfer(t);
    if (rx) rx[i] = r;
  }

  digitalWrite(CS_DEV_B, HIGH);
  SPI.endTransaction();
  busUnlock();
  return true;
}

// ====== Utilidad: parsear 3 int16 de 6 bytes (little-endian) ======
void parse_int16_triplet(const uint8_t* p, int16_t& a, int16_t& b, int16_t& c) {
  a = (int16_t)(p[0] | (p[1] << 8));
  b = (int16_t)(p[2] | (p[3] << 8));
  c = (int16_t)(p[4] | (p[5] << 8));
}

// ====== Tarea de ejemplo con RP2040 ======
void vSpiTask_RP2040(void* pv) {
  int16_t v1 = 100, v2 = -200, v3 = 300;
  uint8_t rx6[6];

  for (;;) {
    bool ok = rp2040_send_packet(v1, v2, v3, rx6, 20);
    if (ok) {
      int16_t r1, r2, r3;
      parse_int16_triplet(rx6, r1, r2, r3);
      Serial.printf("[RP2040] RESP: %d, %d, %d\r\n", r1, r2, r3);
    } else {
      Serial.println("[RP2040] Timeout/FAIL");
    }
    v1 += 1; v2 -= 2; v3 += 3;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ====== Tarea de ejemplo con DEV_B ======
void vSpiTask_DevB(void* pv) {
  uint8_t tx[3] = {0x9F, 0x00, 0x00}; // ej. comando típico "Read ID" de memorias
  uint8_t rx[3];

  for (;;) {
    bool ok = devB_transfer(tx, rx, sizeof(tx));
    if (ok) {
      Serial.printf("[DEV_B] RX: %02X %02X %02X\r\n", rx[0], rx[1], rx[2]);
    } else {
      Serial.println("[DEV_B] FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}