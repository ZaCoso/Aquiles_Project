#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Adafruit_Sensor.h>
namespace Aquiles {

    // ====== Config BLE ===
    constexpr const char* SERVICE_UUID  = "6e400010-b5a3-f393-e0a9-e50e24dcca9e";

    // IMU (6 floats c/u)
    constexpr const char* CHR_IMU_L_UUID = "6e400021-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_IMU_R_UUID = "6e400022-b5a3-f393-e0a9-e50e24dcca9e";

    // Encoders (1 float c/u)  👇 nuevo nombre
    constexpr const char* CHR_ENC_L_UUID = "6e400023-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_ENC_R_UUID = "6e400024-b5a3-f393-e0a9-e50e24dcca9e";

    // 3 floats
    constexpr const char* CHR_F_3_UUID   = "6e400025-b5a3-f393-e0a9-e50e24dcca9e";
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
        // --- Control proporcional de ángulo → velocidad articular ---
        struct AngleCtrlCfg {
        float kp_vel        = 3.0f;   // [rad/s] por [rad]
        float vmax_joint    = 1.5f;   // [rad/s] saturación de v articular
        float deadband_rad  = 0.02f;  // [rad] zona muerta en error angular
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
        Mode  mode     = Mode::VELOCITY;
        };

        // ---------------- Configuración ----------------
        struct LimitsCfg {
        float vel_max = 25.0f;   // rad/s (motor o articular según dónde lo apliques)
        float pos_min = -0.5f;   // rad
        float pos_max =  1.6f;   // rad
        float I_max   =  6.0f;   // (dejalo por si volvés a torque)
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
        AngleCtrlCfg angle;
        float gear_total;
        TimingCfg timing;
        float     slew_per_tick = 0.10f;
      // rampa por ciclo del planner
        };
        void setAngleTarget(Leg leg, float theta_ref_rad);
        float getAngleTarget(Leg leg);

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
    namespace 
    BLE {
        extern BLECharacteristic* chIMU_L;
        extern BLECharacteristic* chIMU_R;
        extern BLECharacteristic* chENC_L;   // antes chF_A
        extern BLECharacteristic* chENC_R;   // antes chF_B
        extern BLECharacteristic* chF_3;
        extern volatile bool deviceConnected;
        extern std::string lastValue;

        class MyCallbacks : public BLECharacteristicCallbacks {
            void onWrite(BLECharacteristic* pCharacteristic) override;
        };

        class MyServerCallbacks : public BLEServerCallbacks {
            void onConnect(BLEServer* pServer) override;
            void onDisconnect(BLEServer* pServer) override;
        };

        void init();
        void enviarEstado();
        void TaskNotify(void* pv);
    }

    namespace UARTDual {
    void init();        // configura Serial1 (L) y Serial2 (R)
    void startTasks();  // crea TX/RX para L y para R
    }

    namespace Tasks {
        void Init(void* params);
        void Blink(void* params);
    }
    namespace Safety {
        void init();           // Inicializa el pin del botón de emergencia
        bool triggered();      // Consulta si se ha presionado el botón
    }

    namespace Power{
    void  init();
    void  setCal(uint8_t tapIdx, float gain, float offs);
    float getCellV(uint8_t i);
    float getCellSOC(uint8_t i);
    float getPackSOC();
    float getPackSOCmin();
  }
    namespace IMU {
        struct Cfg {
        // Mapea qué dirección I2C corresponde a cada pierna
        uint8_t addr_L = 0x68;
        uint8_t addr_R = 0x69;

        // Ejes: cuál es el “forward” (0=x,1=y,2=z) y su signo (+1 o -1) por pierna
        int8_t  fwd_axis_L = 0;  // 0:X,1:Y,2:Z
        int8_t  fwd_sign_L = +1; // +1 o -1
        int8_t  fwd_axis_R = 0;
        int8_t  fwd_sign_R = +1;

        uint16_t period_ms = 5;   // 200 Hz
        };

        bool init(const Cfg& cfg = Cfg{}, TwoWire* bus = &Wire);
        void startTask();
    }
} // namespace Aquiles