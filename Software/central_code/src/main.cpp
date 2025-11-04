// Aquiles main application tasks
// All heavy logic is inside AppTasks.cpp.

#include "AppTasks.h"
using namespace Aquiles;

// ---------- small init helpers to keep setup() tidy ----------

// 1) Emergency / safety
static void initSafety() {
    Aquiles::Safety::init();
}

// 2) Basic RTOS demo tasks (Init, Blink)
static void initBasicTasks() {
    xTaskCreate(Tasks::Init,  "Init",   tskINIT_STACK,  nullptr, tskINIT_PRIORITY,  nullptr);
    xTaskCreate(Tasks::Blink, "Blinky", tskBLINK_STACK, nullptr, tskBLINK_PRIORITY, nullptr);
}

// 3) IMUs on I2C
static void initIMU() {
    Aquiles::IMU::Cfg icfg;
    icfg.addr_L     = 0x68;  // left IMU
    icfg.addr_R     = 0x69;  // right IMU
    icfg.fwd_axis_L = 0;     // X axis forward
    icfg.fwd_axis_R = 0;
    icfg.fwd_sign_L = +1;
    icfg.fwd_sign_R = +1;
    icfg.period_ms  = 5;     // 200 Hz

    // Use Wire1 (or Wire) depending on your board
    Aquiles::IMU::init(icfg, &Wire1);
    Aquiles::IMU::startTask();
}

// 4) BLE telemetry
static void initBLE() {
    Aquiles::BLE::init();
}

// 5) Detection / gait planner
static void initDetection() {
    Aquiles::Detection::GlobalCfg cfg;    // start from defaults
    cfg.intent.omega_min = 1.0f;
    cfg.intent.acc_min_g = 0.30f;
    cfg.limits.I_max     = 6.0f;
    cfg.slew_per_tick    = 0.08f;         // velocity ramp per tick
    Aquiles::Detection::begin(cfg);
}

// 6) Dual UART to both motor modules
static void initUARTDual() {
    Aquiles::UARTDual::init();
    Aquiles::UARTDual::startTasks();
}

// 7) Battery / power monitor
static void initPower() {
    Aquiles::Power::init();
}

// ---------- Arduino setup / loop ----------

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Aquiles booting...");

    // bring up modules in a sensible order
    initSafety();
    initBasicTasks();
    initIMU();
    initBLE();
    initDetection();
    initUARTDual();
    initPower();

    Serial.println("Aquiles ready.");
}

void loop() {
    // Everything runs in FreeRTOS tasks, so main loop can just sleep.
    vTaskDelay(pdMS_TO_TICKS(1000));
}