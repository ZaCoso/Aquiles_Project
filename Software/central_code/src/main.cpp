#include "AppTasks.h"
using namespace Aquiles;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Iniciando...");
    Aquiles::Safety::init();  
    xTaskCreate(Tasks::Init,  "Init",   tskINIT_STACK,  nullptr, tskINIT_PRIORITY,  nullptr);
    xTaskCreate(Tasks::Blink, "Blinky", tskBLINK_STACK, nullptr, tskBLINK_PRIORITY, nullptr);
  // --- IMU on I2C ---
    Aquiles::IMU::Cfg icfg;
    icfg.addr_L = 0x68;            // AD0 a GND
    icfg.addr_R = 0x69;            // AD0 a VCC
    icfg.fwd_axis_L = 0;           // eje X como “forward” L
    icfg.fwd_axis_R = 0;           // eje X como “forward” R
    icfg.fwd_sign_L = +1;          // ajustá según montaje real
    icfg.fwd_sign_R = +1;

    Aquiles::IMU::init(icfg, &Wire /*o &Wire1 si usás ese bus*/);
    Aquiles::IMU::startTask();

    BLE::init();

    Aquiles::Detection::GlobalCfg cfg;      // defaults OK
    cfg.intent.omega_min  = 1.0f;
    cfg.intent.acc_min_g  = 0.30f;
    cfg.limits.I_max      = 6.0f;
    cfg.slew_per_tick     = 0.08f;         // rampa
    Aquiles::Detection::begin(cfg);
    Aquiles::UARTDual::init();
    Aquiles::UARTDual::startTasks();
    Aquiles::Power::init();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}