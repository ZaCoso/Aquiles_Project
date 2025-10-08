#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <NimBLEDevice.h>

namespace Aquiles {

    // ====== Config BLE ======
    constexpr const char* DEVICE_NAME       = "Aquiles X1";
    constexpr const char* SERVICE_UUID      = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHARACTERISTIC_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

    // ====== Pines SPI ======
    constexpr int PIN_SCK   = 18;
    constexpr int PIN_MISO  = 19;
    constexpr int PIN_MOSI  = 23;

    // ====== Chip Selects ======
    constexpr int CS_RP2040 = 5;
    constexpr int CS_DEV_B  = 15;

    // ====== Stack & Priority ======
    constexpr uint16_t tskINIT_STACK      = configMINIMAL_STACK_SIZE;
    constexpr uint16_t tskBLINK_STACK     = 4 * configMINIMAL_STACK_SIZE;
    constexpr uint16_t tskSPI_TX_STACK    = configMINIMAL_STACK_SIZE;

    constexpr UBaseType_t tskINIT_PRIORITY   = tskIDLE_PRIORITY + 3UL;
    constexpr UBaseType_t tskBLINK_PRIORITY  = tskIDLE_PRIORITY + 1UL;
    constexpr UBaseType_t tskSPI_TX_PRIORITY = tskIDLE_PRIORITY + 2UL;

    // ====== Forward namespaces ======
    namespace Detection {

        // ---------------- Tipos básicos ----------------
        enum class Leg   : uint8_t { L=0, R=1 };
        enum class Phase : uint8_t { STANCE=0, SWING=1 };
        enum class Mode  : uint8_t { OFF=0, TORQUE=1, VELOCITY=2, POSITION=3 };

        // Muestra cruda que inyectás vos (una por pierna)
        struct SampleIn {
        float    theta_rad   = 0.0f;   // ángulo tronco–pierna (encoder)
        float    omega_imu   = 0.0f;   // rad/s (gyro) opcional
        float    accel_fwd_g = 0.0f;   // g (acelerómetro) opcional
        uint32_t ts_us       = 0;      // timestamp; si 0 => micros()
        bool     imu_sat     = false;  // saturación IMU
        };

        // Señales derivadas/fusionadas (por pierna)
        struct LegSignals {
        float    theta   = 0.0f;     // rad
        float    omega   = 0.0f;     // rad/s (derivada del encoder)
        float    a_fwd_g = 0.0f;     // g (si se inyecta)
        bool     coh_ok  = true;     // coherencia básica
        uint32_t rx_us   = 0;        // timestamp último update
        };

        // Salidas
        struct IntentOut {
        bool  enable = false;
        Phase phase  = Phase::STANCE;
        };

        struct CmdOut {
        float setpoint = 0.0f;   // lo que mandarías al driver
        Mode  mode     = Mode::TORQUE;
        };

        // ---------------- Configuración ----------------
        struct LimitsCfg {
        float I_max   = 6.0f;     // para modo torque
        float vel_max = 25.0f;    // rad/s
        float pos_min = -3.14f;   // rad
        float pos_max =  3.14f;   // rad
        };

        struct IntentCfg {
        uint16_t t_win_ms   = 100;                          // ventana detección
        uint16_t t_hold_ms  = 50;                           // persistencia
        float    dtheta_min = 10.0f * (float)M_PI/180.f;    // rad (si la usás)
        float    omega_min  = 1.0f;                         // rad/s
        float    acc_min_g  = 0.3f;                         // g
        };

        struct TimingCfg {
        uint16_t fusion_period_ms   = 1;    // 1 kHz
        uint16_t planner_period_ms  = 1;    // 1 kHz
        uint16_t logger_period_ms   = 20;   // 50 Hz
        };

        struct GlobalCfg {
        LimitsCfg limits;
        IntentCfg intent;
        TimingCfg timing;
        float     slew_per_tick = 0.10f;    // rampa por ciclo del planner
        };

        // --------------- API pública -------------------
        // Inicializa la lógica (crea tareas internas). No usa SPI.
        void begin(const GlobalCfg& cfg = GlobalCfg{});

        // Inyectar muestras “en el aire” (desde tus tasks/ISRs)
        void pushSample(Leg leg, const SampleIn& s);

        // Lectura de estado/salidas
        const LegSignals& getSignals(Leg leg);
        CmdOut            getCmd(Leg leg);

        // Ajustes en caliente (opcionales)
        void setLimits(const LimitsCfg& lims);
        void setIntentCfg(const IntentCfg& icfg);
        void setSlew(float slew_per_tick);

        // Hint bilateral (si querés forzar “la otra pierna está en apoyo”)
        void setContraStanceHint(Leg leg_in_swing, bool contralateral_is_in_stance);

    } // namespace Detection
    namespace BLE {
        extern NimBLECharacteristic* pCharacteristic;
        extern volatile bool deviceConnected;
        extern std::string lastValue;

        class MyCallbacks : public NimBLECharacteristicCallbacks {
            void onWrite(NimBLECharacteristic* pCharacteristic) override;
        };

        class MyServerCallbacks : public NimBLEServerCallbacks {
            void onConnect(NimBLEServer* pServer) override;
            void onDisconnect(NimBLEServer* pServer) override;
        };

        void init();
        void enviarEstado();
        void TaskNotify(void* pv);
    }

    namespace SPIBus {
        void init();
        bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                                uint8_t resp6[6], uint32_t timeout_ms = 20);
        bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n);
        uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init = 0x00);
        void parse_triplet(const uint8_t* p, int16_t& a, int16_t& b, int16_t& c);

        void vSpiTask_RP2040(void* pv);
        void vSpiTask_DevB(void* pv);
    }

    namespace Tasks {
        void Init(void* params);
        void Blink(void* params);
    }

} // namespace Aquiles