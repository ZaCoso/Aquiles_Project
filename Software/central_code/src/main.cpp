#include "AppTasks.h"
using namespace Aquiles;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Iniciando...");

    xTaskCreate(Tasks::Init,  "Init",   tskINIT_STACK,  nullptr, tskINIT_PRIORITY,  nullptr);
    xTaskCreate(Tasks::Blink, "Blinky", tskBLINK_STACK, nullptr, tskBLINK_PRIORITY, nullptr);

    BLE::init();

    // Si querés activar SPI:
    // SPIBus::init();
    // xTaskCreate(SPIBus::vSpiTask_RP2040, "SPI_RP2040", tskSPI_TX_STACK, nullptr, tskSPI_TX_PRIORITY, nullptr);
    // xTaskCreate(SPIBus::vSpiTask_DevB,   "SPI_DEV_B",  tskSPI_TX_STACK, nullptr, tskSPI_TX_PRIORITY, nullptr);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}