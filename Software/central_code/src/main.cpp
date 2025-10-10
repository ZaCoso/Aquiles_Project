#include "AppTasks.h"
using namespace Aquiles;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Iniciando...");

    xTaskCreate(Tasks::Init,  "Init",   tskINIT_STACK,  nullptr, tskINIT_PRIORITY,  nullptr);
    xTaskCreate(Tasks::Blink, "Blinky", tskBLINK_STACK, nullptr, tskBLINK_PRIORITY, nullptr);

    BLE::init();

    Aquiles::Detection::GlobalCfg cfg;      // defaults OK
    cfg.intent.omega_min  = 1.0f;
    cfg.intent.acc_min_g  = 0.30f;
    cfg.limits.I_max      = 6.0f;
    cfg.slew_per_tick     = 0.08f;         // rampa
    Aquiles::Detection::begin(cfg);
    Aquiles::SPIBus::init();
    Aquiles::SPIBus::startTickTask();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}