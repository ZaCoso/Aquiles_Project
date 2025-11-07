#pragma once
// Main header for the Aquiles firmware.

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <Wire.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

namespace Aquiles {

    // =========================================================
    // Device identity & BLE UUIDs
    // =========================================================
    // Name shown over BLE scanning
    extern const char DEVICE_NAME[];

    // Main BLE service for this device
    constexpr const char* SERVICE_UUID   = "6e400010-b5a3-f393-e0a9-e50e24dcca9e";

    // Characteristics (we use several small ones instead of a single big blob)
    constexpr const char* CHR_IMU_L_UUID = "6e400021-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_IMU_R_UUID = "6e400022-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_ENC_L_UUID = "6e400023-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_ENC_R_UUID = "6e400024-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHR_F_3_UUID   = "6e400025-b5a3-f393-e0a9-e50e24dcca9e";

    // =========================================================
    // Task stacks & priorities used in main
    // =========================================================
    constexpr uint16_t    tskINIT_STACK      = configMINIMAL_STACK_SIZE;
    constexpr uint16_t    tskBLINK_STACK     = 4 * configMINIMAL_STACK_SIZE;
    constexpr uint16_t    tskSPI_TX_STACK    = configMINIMAL_STACK_SIZE;

    constexpr UBaseType_t tskINIT_PRIORITY   = tskIDLE_PRIORITY + 3UL;
    constexpr UBaseType_t tskBLINK_PRIORITY  = tskIDLE_PRIORITY + 1UL;
    constexpr UBaseType_t tskSPI_TX_PRIORITY = tskIDLE_PRIORITY + 2UL;

    // =========================================================
    // 1. Detection / Gait planner
    //    This is the core “logic” that fuses sensors and decides
    //    what to command to each leg.
    // =========================================================
    namespace Detection {

        // Which leg we’re talking about
        enum class Leg   : uint8_t { L = 0, R = 1 };
        // Gait phase (you can extend this later)
        enum class Phase : uint8_t { STANCE = 0, SWING = 1 };
        // Control mode we send to the driver
        enum class Mode  : uint8_t { OFF = 0, TORQUE = 1, VELOCITY = 2, POSITION = 3 };

        // Raw sample coming from either UART (encoder) or IMU (acc/gyro)
        struct SampleIn {
            float    theta_rad   = 0.0f;   // joint angle (or equivalent)
            float    omega_imu   = 0.0f;   // angular speed from IMU (optional)
            float    accel_fwd_g = 0.0f;   // forward accel in g (optional)
            uint32_t ts_us       = 0;      // timestamp in µs (0 = use micros())
            bool     imu_sat     = false;  // IMU saturation flag
        };

        // Angle-to-velocity controller config
        struct AngleCtrlCfg {
            float kp_vel       = 3.0f;   // [rad/s] per [rad] of error
            float vmax_joint   = 1.5f;   // joint vel clamp
            float deadband_rad = 0.02f;  // do nothing for tiny errors
        };

        // Per-leg fused signals (what the planner reads)
        struct LegSignals {
            float    theta   = 0.0f;     // unwrapped angle
            float    omega   = 0.0f;     // encoder-derived velocity
            float    a_fwd_g = 0.0f;     // forward accel
            bool     coh_ok  = true;     // basic coherence flag
            uint32_t rx_us   = 0;        // last update timestamp
        };

        // Output of intention detector
        struct IntentOut {
            bool  enable = false;        // should we assist now?
            Phase phase  = Phase::STANCE;
        };

        // Final command to the motor driver
        struct CmdOut {
            float setpoint = 0.0f;       // usually a velocity setpoint
            Mode  mode     = Mode::VELOCITY;
        };

        // Safety/kinematic limits
        struct LimitsCfg {
            float vel_max = 25.0f;       // rad/s (motor or joint)
            float pos_min = -0.5f;       // lower joint limit
            float pos_max =  1.6f;       // upper joint limit
            float I_max   =  6.0f;       // kept for torque mode
        };

        // Intention detection thresholds
        struct IntentCfg {
            uint16_t t_win_ms   = 100;
            uint16_t t_hold_ms  = 50;
            float    dtheta_min = 10.0f * (float)M_PI / 180.f;  // 10°
            float    omega_min  = 1.0f;
            float    acc_min_g  = 0.3f;
        };

        // Internal task periods
        struct TimingCfg {
            uint16_t fusion_period_ms  = 1;   // 1 kHz
            uint16_t planner_period_ms = 1;   // 1 kHz
            uint16_t logger_period_ms  = 20;  // 50 Hz
        };

        // Global configuration passed once in begin()
        struct GlobalCfg {
            LimitsCfg    limits;
            IntentCfg    intent;
            AngleCtrlCfg angle;
            float        gear_total     = 1.0f; // if you need motor→joint conversion
            TimingCfg    timing;
            float        slew_per_tick  = 0.10f; // velocity ramp per planner tick
        };

        // ---- Public API ----
        void  begin(const GlobalCfg& cfg = GlobalCfg{});
        void  pushSample(Leg leg, const SampleIn& s);
        const LegSignals& getSignals(Leg leg);
        CmdOut            getCmd(Leg leg);

        void setLimits(const LimitsCfg& lims);
        void setIntentCfg(const IntentCfg& icfg);
        void setSlew(float slew_per_tick);
        void setContraStanceHint(Leg leg_in_swing, bool contralateral_is_in_stance);

        void  setAngleTarget(Leg leg, float theta_ref_rad);
        float getAngleTarget(Leg leg);

        // Logging control
        void  enableLogging(bool enable = true);
        bool  isLoggingEnabled();
    } // namespace Detection

    // =========================================================
    // 2. BLE – telemetry & simple control channel
    // =========================================================
    namespace BLE {

        // Exposed so other modules can set values
        extern BLECharacteristic* chIMU_L;
        extern BLECharacteristic* chIMU_R;
        extern BLECharacteristic* chENC_L;
        extern BLECharacteristic* chENC_R;
        extern BLECharacteristic* chF_3;

        // Set when a phone is connected
        extern volatile bool      deviceConnected;
        // Last string received (for basic commands)
        extern std::string        lastValue;

        // Called when a BLE characteristic is written
        class MyCallbacks : public BLECharacteristicCallbacks {
            void onWrite(BLECharacteristic* pCharacteristic) override;
        };

        // Called on connect/disconnect
        class MyServerCallbacks : public BLEServerCallbacks {
            void onConnect(BLEServer* pServer) override;
            void onDisconnect(BLEServer* pServer) override;
        };

        // Start BLE stack, service, characteristics and notify task
        void init();
        // Example “compact status” sender (3 floats)
        void enviarEstado();
        // Periodic notify task
        void TaskNotify(void* pv);
    } // namespace BLE

    // =========================================================
    // 3. Dual-UART: one link per leg
    // =========================================================
    namespace UARTDual {
        // Configure UARTs, pins, baudrate, contexts
        void init();
        // Spawn TX/RX tasks for L and R
        void startTasks();
    }

    // =========================================================
    // 4. Basic tasks (blinky, init)
    // =========================================================
    namespace Tasks {
        void Init(void* params);
        void Blink(void* params);
    }

    // =========================================================
    // 5. Safety / E-STOP
    // =========================================================
    namespace Safety {
        void init();      // setup pin + interrupt
        bool triggered(); // returns true if E-STOP was hit
    }

    // =========================================================
    // 6. IMU (two MPU6050 on same bus, different addresses)
    // =========================================================
    namespace IMU {

        // How IMUs are mounted
        struct Cfg {
            uint8_t  addr_L     = 0x68;
            uint8_t  addr_R     = 0x69;
            int8_t   fwd_axis_L = 0;     // which axis is “forward” on L IMU
            int8_t   fwd_sign_L = +1;    // axis sign
            int8_t   fwd_axis_R = 0;
            int8_t   fwd_sign_R = +1;
            uint16_t period_ms  = 5;     // read every 5 ms (200 Hz)
        };

        bool init(const Cfg& cfg = Cfg{}, TwoWire* bus = &Wire);
        void startTask();
    }

    // =========================================================
    // 7. Power / Battery monitor (4 taps → 4 cells)
    // =========================================================
    namespace Power {
        void  init();
        void  setCal(uint8_t tapIdx, float gain, float offs);
        float getCellV(uint8_t i);
        // used by BLE status
        float getPackSOC();
        float getPackSOCmin();
    }

} // namespace Aquiles