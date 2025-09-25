#include "AppTasks.h"

///Semaphore
xSemaphoreHandle semph_LeftMotor;
xSemaphoreHandle semph_RightMotor;

/// Queues

void InitTask(void *params){
    semph_LeftMotor = xSemaphoreCreateBinary();
    semph_RightMotor = xSemaphoreCreateBinary();
    pinMode(46, OUTPUT);
    vTaskDelete(NULL);
    Serial.begin(115200);
    pinMode(PIN_CS, OUTPUT);
    digitalWrite(PIN_CS, HIGH);

  // Iniciar bus VSPI (NO en otra tarea antes que ésta)
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

}
void BlinkTask(void *params){
    while(1){
    digitalWrite(46, !digitalRead(46));
    vTaskDelay(pdMS_TO_TICKS(500));}
}
// ---- Tarea que envía el paquete SPI cada 500 ms ----
void vSpiTxTask(void* params) {
  // Arrancamos la transacción SPI en esta tarea
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (;;) {
    // Tus 3 enteros
    int16_t v1 = 1234;
    int16_t v2 = -456;
    int16_t v3 = 789;

    uint8_t payload[6] = {
      (uint8_t)(v1 & 0xFF), (uint8_t)((v1 >> 8) & 0xFF),
      (uint8_t)(v2 & 0xFF), (uint8_t)((v2 >> 8) & 0xFF),
      (uint8_t)(v3 & 0xFF), (uint8_t)((v3 >> 8) & 0xFF)
    };

    uint8_t buf[9];
    buf[0] = 0xAA;  // SOF
    buf[1] = 6;     // LEN
    memcpy(&buf[2], payload, 6);
    buf[8] = crc8_sum(payload, 6);


    digitalWrite(PIN_CS, LOW);
    for (size_t i = 0; i < sizeof(buf); ++i) {
      (void)SPI.transfer(buf[i]);  // opcional: leer respuesta del slave
    }
    digitalWrite(PIN_CS, HIGH);

    // Log no muy frecuente para no bloquear
    static uint32_t n = 0;
    if ((n++ % 10) == 0) Serial.println("[SPI] Paquete enviado");
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  // Nunca llega, pero por prolijidad:
  SPI.endTransaction();
  vTaskDelete(nullptr);
}
