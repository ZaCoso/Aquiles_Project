#include "AppTasks.h"
#include <cmath>
#include <esp_timer.h>  // for esp_timer_get_time() on ESP32

const char Aquiles::DEVICE_NAME[] = "Aquiles X1";

namespace Aquiles {

// =========================================================
// 1. DETECTION / GAIT PLANNER IMPLEMENTATION
// =========================================================
namespace Detection {

    // ----- Internal state for this module -----
    static GlobalCfg CFG;        // current configuration

    // one inbound queue per leg (depth 1, overwrite)
    static QueueHandle_t qInL = nullptr;
    static QueueHandle_t qInR = nullptr;

    // fused signals per leg
    static LegSignals SIG_L, SIG_R;
    // final commands per leg
    static CmdOut     CMD_L, CMD_R;
    // per-leg smoothed setpoints
    static float      sp_cur_L = 0.0f, sp_cur_R = 0.0f;

    // bilateral swing/stance hint (optional)
    static bool contra_hint_L = true;  // “R is in stance”
    static bool contra_hint_R = true;  // “L is in stance”

    // angle targets per leg
    static float theta_ref_L = 0.0f;
    static float theta_ref_R = 0.0f;

    // logging control
    static volatile bool logging_enabled = false;

    // ----- Small helpers -----
    static inline uint32_t micros32() { return (uint32_t)micros(); }

    // unwrap angle to avoid jumps of ±pi
    static inline float unwrap(float th, float th_prev) {
        float d = th - th_prev;
        while (d >  (float)M_PI) d -= 2.f * (float)M_PI;
        while (d < -(float)M_PI) d += 2.f * (float)M_PI;
        return th_prev + d;
    }

    // generic slew-rate limiter
    static inline float slew(float target, float current, float max_delta){
        float d = target - current;
        if (d >  max_delta) d =  max_delta;
        if (d < -max_delta) d = -max_delta;
        return current + d;
    }

    // clamp helper
    static inline float clamp(float v, float lo, float hi){
        return (v < lo) ? lo : (v > hi) ? hi : v;
    }

    // public: set angle target per leg
    void setAngleTarget(Leg leg, float theta_ref_rad) {
        if (leg == Leg::L) theta_ref_L = theta_ref_rad;
        else               theta_ref_R = theta_ref_rad;
    }

    // public: get angle target per leg
    float getAngleTarget(Leg leg){
        return (leg == Leg::L) ? theta_ref_L : theta_ref_R;
    }

    // ----- Intention detection -----
    // very simple: enough movement on this leg + contralateral quiet
    static IntentOut detectIntent(const LegSignals& s_i,
                                  const LegSignals& s_contra,
                                  const IntentCfg& icfg,
                                  bool contra_stance_hint)
    {
        // movement evidence (angular vel and/or accel)
        bool geom   = (fabsf(s_i.omega) >= icfg.omega_min);
        bool inert  = (fabsf(s_i.a_fwd_g) >= icfg.acc_min_g) || geom;
        // contralateral stance evidence
        bool contra = contra_stance_hint || (fabsf(s_contra.omega) < 0.2f);
        // temporal persistence (you can add counters later)
        bool persist= true;

        IntentOut out;
        out.enable = (geom && inert && contra && persist);
        out.phase  = out.enable ? Phase::SWING : Phase::STANCE;
        return out;
    }

    // ----- Planner: angle-P → joint velocity -----
    static CmdOut planner_angleP(const IntentOut& /*I*/,
                                 const LegSignals& s,
                                 const LimitsCfg& lims,
                                 const GlobalCfg& C,
                                 float& sp_accum)
    {
        // find which leg this signal struct belongs to
        float theta_ref = (&s == &SIG_L) ? theta_ref_L : theta_ref_R;

        // angle error
        float err = theta_ref - s.theta;

        // soft joint window
        if (s.theta < lims.pos_min) err = lims.pos_min - s.theta;
        if (s.theta > lims.pos_max) err = lims.pos_max - s.theta;

        // deadband to prevent hunting
        if (fabsf(err) < C.angle.deadband_rad) err = 0.0f;

        // P controller → desired joint velocity
        float v_joint = C.angle.kp_vel * err;
        v_joint = clamp(v_joint, -C.angle.vmax_joint, C.angle.vmax_joint);

        // smooth the velocity with a per-tick slew
        sp_accum = slew(v_joint, sp_accum, C.slew_per_tick);

        // build command
        CmdOut out;
        out.mode     = Mode::VELOCITY;
        out.setpoint = sp_accum;
        return out;
    }

    // ----- Task: fusion (1 kHz) -----
    // pulls latest samples for each leg and computes theta/omega
    static void taskFusion(void*){
        float    thL_prev = 0.0f, thR_prev = 0.0f;
        uint32_t tsL_prev = 0,     tsR_prev = 0;
        TickType_t last   = xTaskGetTickCount();

        for(;;){
            // ---- LEFT LEG ----
            SampleIn inL;
            if (xQueueReceive(qInL, &inL, 0) == pdTRUE){
                uint32_t ts = inL.ts_us ? inL.ts_us : micros32();

                // update angle & omega
                if (!isnan(inL.theta_rad)){
                    float th = unwrap(inL.theta_rad, thL_prev);
                    float dt = (tsL_prev==0) ? 0.001f : (float)(ts - tsL_prev)/1e6f;
                    if (dt <= 0) dt = 1e-3f;
                    float w  = (th - thL_prev)/dt;

                    SIG_L.theta = th;
                    SIG_L.omega = w;
                    thL_prev = th; tsL_prev = ts;
                }

                // update accel if present
                if (!isnan(inL.accel_fwd_g)) SIG_L.a_fwd_g = inL.accel_fwd_g;
                // mark coh flag if IMU was saturated
                if (inL.imu_sat) SIG_L.coh_ok = false;

                SIG_L.rx_us = ts;
            }

            // ---- RIGHT LEG ----
            SampleIn inR;
            if (xQueueReceive(qInR, &inR, 0) == pdTRUE){
                uint32_t ts = inR.ts_us ? inR.ts_us : micros32();

                if (!isnan(inR.theta_rad)){
                    float th = unwrap(inR.theta_rad, thR_prev);
                    float dt = (tsR_prev==0) ? 0.001f : (float)(ts - tsR_prev)/1e6f;
                    if (dt <= 0) dt = 1e-3f;
                    float w  = (th - thR_prev)/dt;

                    SIG_R.theta = th;
                    SIG_R.omega = w;
                    thR_prev = th; tsR_prev = ts;
                }

                if (!isnan(inR.accel_fwd_g)) SIG_R.a_fwd_g = inR.accel_fwd_g;
                if (inR.imu_sat) SIG_R.coh_ok = false;

                SIG_R.rx_us = ts;
            }

            vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.fusion_period_ms));
        }
    }

    // ----- Task: planner (1 kHz) -----
    static void taskPlanner(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
            // intention on both legs (using bilateral hint)
            IntentOut IL = detectIntent(SIG_L, SIG_R, CFG.intent, contra_hint_L);
            IntentOut IR = detectIntent(SIG_R, SIG_L, CFG.intent, contra_hint_R);

            // per-leg planning
            CMD_L = planner_angleP(IL, SIG_L, CFG.limits, CFG, sp_cur_L);
            CMD_R = planner_angleP(IR, SIG_R, CFG.limits, CFG, sp_cur_R);

            vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.planner_period_ms));
        }
    }

    // ----- Task: logger (50 Hz) -----
    static void taskLogger(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
            if (logging_enabled) {
                // Intention detection results
                IntentOut IL = detectIntent(SIG_L, SIG_R, CFG.intent, contra_hint_L);
                IntentOut IR = detectIntent(SIG_R, SIG_L, CFG.intent, contra_hint_R);

                // Log detection inputs and results
                Serial.printf("[DET] L: th=%.3f w=%.3f a=%.3fg | R: th=%.3f w=%.3f a=%.3fg\n",
                    SIG_L.theta, SIG_L.omega, SIG_L.a_fwd_g,
                    SIG_R.theta, SIG_R.omega, SIG_R.a_fwd_g);

                // Log intention detection results
                Serial.printf("[INT] L: enable=%d phase=%s | R: enable=%d phase=%s\n",
                    IL.enable ? 1 : 0, (IL.phase == Phase::SWING) ? "SWING" : "STANCE",
                    IR.enable ? 1 : 0, (IR.phase == Phase::SWING) ? "SWING" : "STANCE");

                // Log command outputs
                Serial.printf("[CMD] L: mode=%d sp=%.3f | R: mode=%d sp=%.3f | ref_L=%.3f ref_R=%.3f\n",
                    (int)CMD_L.mode, CMD_L.setpoint,
                    (int)CMD_R.mode, CMD_R.setpoint,
                    theta_ref_L, theta_ref_R);
            }
            vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.logger_period_ms));
        }
    }

    // ----- Public entry point -----
    void begin(const GlobalCfg& cfg){
        CFG = cfg;

        // create inbound queues (one per leg)
        qInL = xQueueCreate(1, sizeof(SampleIn));
        qInR = xQueueCreate(1, sizeof(SampleIn));

        // spawn internal tasks
        xTaskCreatePinnedToCore(taskFusion,  "Fusion",  3072, nullptr, 4, nullptr, 1);
        xTaskCreatePinnedToCore(taskPlanner, "Planner", 4096, nullptr, 3, nullptr, 1);
        xTaskCreatePinnedToCore(taskLogger,  "Logger",  2048, nullptr, 1, nullptr, 0);
    }

    void pushSample(Leg leg, const SampleIn& s){
        // overwrite so the latest sample always wins
        if (leg == Leg::L) xQueueOverwrite(qInL, &s);
        else               xQueueOverwrite(qInR, &s);
    }

    const LegSignals& getSignals(Leg leg){
        return (leg == Leg::L) ? SIG_L : SIG_R;
    }

    CmdOut getCmd(Leg leg){
        return (leg == Leg::L) ? CMD_L : CMD_R;
    }

    void setLimits(const LimitsCfg& lims){ CFG.limits = lims; }
    void setIntentCfg(const IntentCfg& icfg){ CFG.intent = icfg; }
    void setSlew(float slew_per_tick){ CFG.slew_per_tick = slew_per_tick; }

    void setContraStanceHint(Leg leg_in_swing, bool contralateral_is_in_stance){
        if (leg_in_swing == Leg::L) contra_hint_L = contralateral_is_in_stance;
        else                        contra_hint_R = contralateral_is_in_stance;
    }

    // Logging control
    void enableLogging(bool enable){
        logging_enabled = enable;
        Serial.printf("[Detection] Logging %s\n", enable ? "ENABLED" : "DISABLED");
    }

    bool isLoggingEnabled(){
        return logging_enabled;
    }

} // namespace Detection



// =========================================================
// 2. BLE IMPLEMENTATION
// =========================================================
namespace BLE {

    // exposed in header
    BLECharacteristic* chIMU_L = nullptr;
    BLECharacteristic* chIMU_R = nullptr;
    BLECharacteristic* chENC_L = nullptr;
    BLECharacteristic* chENC_R = nullptr;
    BLECharacteristic* chF_3   = nullptr;
    BLECharacteristic* chCMD   = nullptr;

    volatile bool deviceConnected = false;
    volatile bool streamEnabled   = false;
    static   volatile bool mockEnabled     = true;   // demo mode for BLE without real data
    std::string   lastValue = "0";

    // Buzzer pin (active buzzer on GPIO 11)
    static constexpr int PIN_BUZZER = 11;
    static volatile bool g_buzzing = false;

    // Small one-shot task to beep for 1 second without blocking BLE callback
    static void vBuzzerBeep1s(void*){
        digitalWrite(PIN_BUZZER, HIGH);
        vTaskDelay(pdMS_TO_TICKS(1000));
        digitalWrite(PIN_BUZZER, LOW);
        g_buzzing = false;
        vTaskDelete(nullptr);
    }

    // Optional compact payloads (not used in string example)
    #pragma pack(push,1)
    struct IMU6F  { float ax, ay, az, gx, gy, gz; };
    struct ONEF   { float a; };
    struct THREEF { float a, b, c; };
    #pragma pack(pop)

    // simple settings (mirrors mock sketch keys)
    struct Settings { uint8_t AID=50, RES=50, SEN=50; } settings;

    // called when phone writes to the command characteristic
    void MyCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
        String s = String(pCharacteristic->getValue().c_str());
        s.trim();
        
        // log raw
        if (s.length() > 0) {
            Serial.print("[BLE CMD RAW] ");
            Serial.println(s);
            lastValue = std::string(s.c_str());
        } else {
            return;
        }

        // beep non-blocking
        if (!g_buzzing){
            g_buzzing = true;
            xTaskCreatePinnedToCore(vBuzzerBeep1s, "Buzz1s", 1024, nullptr, 1, nullptr, 0);
        }

        // Basic triggers
        if (s == "1" || s.equalsIgnoreCase("ON")) {
            streamEnabled = true;
            Serial.println("[ACTION] Streaming ENABLED");
            return;
        }
        if (s.equalsIgnoreCase("OFF")) {
            streamEnabled = false;
            Serial.println("[ACTION] Streaming DISABLED");
            return;
        }

        // Parse CMD:VALUE
        int colon = s.indexOf(':');
        if (colon > 0) {
            String cmd = s.substring(0, colon); cmd.trim(); cmd.toUpperCase();
            String val = s.substring(colon + 1); val.trim();

            auto clamp0100 = [](int v) { return v<0?0:(v>100?100:v); };

            if (cmd == "AID") {
                settings.AID = clamp0100(val.toInt());
                Serial.printf("[CMD] AID = %u (Assistance)\n", settings.AID);
            }
            else if (cmd == "RES") {
                settings.RES = clamp0100(val.toInt());
                Serial.printf("[CMD] RES = %u (Responsiveness)\n", settings.RES);
            }
            else if (cmd == "SEN") {
                settings.SEN = clamp0100(val.toInt());
                Serial.printf("[CMD] SEN = %u (Sensitivity)\n", settings.SEN);
            }
            else if (cmd == "SPD") {
                Serial.printf("[CMD] SPD = %s (Mock speed)\n", val.c_str());
            }
            else if (cmd == "MODE") {
                Serial.printf("[CMD] MODE = %s (Mock mode)\n", val.c_str());
            }
            else if (cmd == "CAL") {
                Serial.println("[CMD] CAL request (Mock calibration)");
            }
            else if (cmd == "BAT") {
                Serial.println("[CMD] BAT request (Mock battery)");
            }
            else if (cmd == "STA") {
                Serial.println("[CMD] STA request (Mock status)");
            }
            else if (cmd == "DEMO" || cmd == "MOCK") {
                String vup = val; vup.toUpperCase();
                if (vup == "ON" || vup == "1") { 
                    mockEnabled = true;  
                    Serial.println("[ACTION] DEMO MODE ON"); 
                }
                else if (vup == "OFF" || vup == "0") { 
                    mockEnabled = false; 
                    Serial.println("[ACTION] DEMO MODE OFF"); 
                }
                else { 
                    Serial.printf("[CMD] DEMO expects ON/OFF, got '%s'\n", val.c_str()); 
                }
            }
            else {
                Serial.printf("[CMD] Unknown key '%s' with value '%s'\n", cmd.c_str(), val.c_str());
            }
            return;
        }

        // Non-colon commands
        String up = s; up.toUpperCase();
        if (up == "CAL" || up == "BAT" || up == "STA") {
            Serial.printf("[CMD] %s (mock single-word)\n", up.c_str());
        } else if (up == "DEMO" || up == "MOCK") {
            mockEnabled = !mockEnabled;
            Serial.printf("[ACTION] DEMO toggled %s\n", mockEnabled?"ON":"OFF");
        } else {
            Serial.printf("[CMD] Unrecognized single token: '%s'\n", s.c_str());
        }
    }

    void MyServerCallbacks::onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }

    void MyServerCallbacks::onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        streamEnabled = false;
        Serial.println("[BLE] Client disconnected");
        pServer->getAdvertising()->start();
    }

    // periodic notify task (20 Hz)
    void TaskNotify(void*) {
        const TickType_t period = pdMS_TO_TICKS(50);
        static float tphase = 0.0f;            // used in mock mode
        for(;;){
            if (deviceConnected && streamEnabled) {
                // pull signals from detection
                const auto& SL = Aquiles::Detection::getSignals(Aquiles::Detection::Leg::L);
                const auto& SR = Aquiles::Detection::getSignals(Aquiles::Detection::Leg::R);

                // encoder left (binary float)
                if (chENC_L) {
                    float th = mockEnabled ? (45.0f * (float)M_PI/180.0f + 40.0f * (float)M_PI/180.0f * sinf(tphase + 0.2f)) : SL.theta;
                    ONEF f{ th };
                    chENC_L->setValue((uint8_t*)&f, sizeof(f));
                    chENC_L->notify();
                }

                // encoder right (binary float)
                if (chENC_R) {
                    float th = mockEnabled ? (45.0f * (float)M_PI/180.0f + 40.0f * (float)M_PI/180.0f * sinf(tphase + 1.3f)) : SR.theta;
                    ONEF f{ th };
                    chENC_R->setValue((uint8_t*)&f, sizeof(f));
                    chENC_R->notify();
                }

                // IMU left (6 floats). Populate available fields; rest zero
                if (chIMU_L) {
                    IMU6F f{};
                    if (mockEnabled){
                        f.ax = 0.1f * sinf(tphase + 0.0f);
                        f.ay = 0.2f * cosf(tphase * 0.8f + 0.0f);
                        f.az = 1.00f + 0.02f * sinf(tphase * 0.4f + 0.0f);
                        f.gx = 10.0f * sinf(tphase * 1.5f + 0.0f);
                        f.gy = 12.0f * cosf(tphase * 1.2f + 0.0f);
                        f.gz =  8.0f * sinf(tphase * 0.9f + 0.0f);
                    } else {
                        f.ax = SL.a_fwd_g;   // use g-units for now
                        f.gx = SL.omega;     // rad/s from detection
                    }
                    chIMU_L->setValue((uint8_t*)&f, sizeof(f));
                    chIMU_L->notify();
                }

                // IMU right (6 floats)
                if (chIMU_R) {
                    IMU6F f{};
                    if (mockEnabled){
                        f.ax = 0.1f * sinf(tphase + 1.1f);
                        f.ay = 0.2f * cosf(tphase * 0.8f + 1.1f);
                        f.az = 1.00f + 0.02f * sinf(tphase * 0.4f + 1.1f);
                        f.gx = 10.0f * sinf(tphase * 1.5f + 1.1f);
                        f.gy = 12.0f * cosf(tphase * 1.2f + 1.1f);
                        f.gz =  8.0f * sinf(tphase * 0.9f + 1.1f);
                    } else {
                        f.ax = SR.a_fwd_g;
                        f.gx = SR.omega;
                    }
                    chIMU_R->setValue((uint8_t*)&f, sizeof(f));
                    chIMU_R->notify();
                }

                // Compact status / forces (3 floats). Keep battery-style for now
                if (chF_3) {
                    THREEF f3{};
                    if (mockEnabled){
                        f3.a =  5.0f + 2.5f * sinf(tphase * 0.7f);
                        f3.b = -1.5f + 2.0f * cosf(tphase * 0.6f);
                        f3.c = 12.0f + 3.0f * sinf(tphase * 0.5f);
                    } else {
                        f3.a = Power::getPackSOC();
                        f3.b = Power::getPackSOCmin();
                        f3.c = 1.0f;
                    }
                    chF_3->setValue((uint8_t*)&f3, sizeof(f3));
                    chF_3->notify();
                }
                // advance phase for mock
                tphase += 0.02f;
            }
            vTaskDelay(period);
        }
    }

    // full BLE init: server, service, characteristics, advertising, notify task
    void init() {
        Serial.println("\n[Aquiles x1] ESP32 BLE Arduino – Verbose Command Logging");

        // start BLE with device name
        BLEDevice::init(DEVICE_NAME);
        BLEDevice::setMTU(185);

        // create server and set callbacks
        BLEServer* pServer = BLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        // create service
        BLEService* pService = pServer->createService(SERVICE_UUID);

        // create command characteristic (write-only)
        chCMD = pService->createCharacteristic(CHR_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
        chCMD->setCallbacks(new MyCallbacks());

        // create data characteristics (notify-only)
        chIMU_L = pService->createCharacteristic(CHR_IMU_L_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        chIMU_R = pService->createCharacteristic(CHR_IMU_R_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        chENC_L = pService->createCharacteristic(CHR_ENC_L_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        chENC_R = pService->createCharacteristic(CHR_ENC_R_UUID, BLECharacteristic::PROPERTY_NOTIFY);
        chF_3   = pService->createCharacteristic(CHR_F_3_UUID, BLECharacteristic::PROPERTY_NOTIFY);

        // add BLE2902 descriptor to all notify characteristics
        chIMU_L->addDescriptor(new BLE2902());
        chIMU_R->addDescriptor(new BLE2902());
        chENC_L->addDescriptor(new BLE2902());
        chENC_R->addDescriptor(new BLE2902());
        chF_3->addDescriptor(new BLE2902());

        // start service
        pService->start();

        // configure advertising
        BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->setMinPreferred(0x06);
        pAdvertising->setMinPreferred(0x12);
        pAdvertising->start();

        Serial.println("[BLE] Advertising as 'Aquiles X1'");

        // configure buzzer pin
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);

        // create notify task
        xTaskCreatePinnedToCore(
            TaskNotify,
            "BLE_NOTIFY",
            4096,
            nullptr,
            1,
            nullptr,
            1
        );
    }

    // tiny helper to send 3 floats
    void enviarEstado() {
        if (!deviceConnected || !chF_3) return;
        THREEF f3{ Power::getPackSOC(),
                   Power::getPackSOCmin(),
                   1.0f /* placeholder health */ };
        chF_3->setValue((uint8_t*)&f3, sizeof(f3));
        chF_3->notify();
    }

} // namespace BLE



// =========================================================
// 3. UARTDUAL IMPLEMENTATION
// =========================================================
namespace UARTDual {

    // UART config
    static constexpr uint32_t UART_BAUD = 2000000; // 2 Mbps

    // pins for each link
    static constexpr int L_TX = 5;
    static constexpr int L_RX = 6;
    static constexpr int R_TX = 7;
    static constexpr int R_RX = 15;

    // use Serial1 and Serial2 from Arduino core
    HardwareSerial& U_L = Serial1;
    HardwareSerial& U_R = Serial2;

    // protocol constants
    static constexpr uint8_t PROTO_VERSION_V2 = 0x02;
    static constexpr uint8_t ACT_SOF         = 0x5A;
    static constexpr uint8_t TEL_SOF         = 0xA5;
    static constexpr size_t  ACT_SIZE        = 16;

    // SLIP constants
    static constexpr uint8_t SLIP_END     = 0xC0;
    static constexpr uint8_t SLIP_ESC     = 0xDB;
    static constexpr uint8_t SLIP_ESC_END = 0xDC;
    static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

    // ---- frames ----
    #pragma pack(push,1)
    // host → module
    struct ActuationCmdV1 {
        uint8_t  sof, version, len, seq;
        uint8_t  mode, rsv0;
        int16_t  tgt_q15, kp_q15, kd_q15;
        uint16_t limits;
        uint8_t  rsv1, crc8;
    };
    // module → host
    struct TelemetryFrameV2 {
        uint8_t  sof, version, len, seq;
        uint32_t t_us;
        uint16_t flags, sample_hz;
        int16_t  enc_q15;
        int16_t  ipeak_q15;
        uint8_t  phase_peak;
        uint16_t frame_ctr;
        uint8_t  crc8;
    };
    #pragma pack(pop)

    static constexpr size_t  TEL2_SIZE = sizeof(TelemetryFrameV2);
    static constexpr uint8_t TEL2_LEN  = TEL2_SIZE - 1;

    // ---- protocol helpers ----
    static uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init){
        uint8_t crc = init;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (uint8_t k = 0; k < 8; ++k)
                crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
        }
        return crc;
    }

    static float   q15_to_rad(int16_t q){ return (float)q * 2.0f * (float)M_PI / 32768.0f; }
    static int16_t vel_to_q15(float vel_rad_s){ return (int16_t)lrintf(vel_rad_s * 2048.0f); }

    // build actuation frame in driver’s format
    static void buildActCmd(ActuationCmdV1& c, uint8_t seq, float v_joint_setpoint_rad_s){
        memset(&c, 0, sizeof(c));
        c.sof     = ACT_SOF;
        c.version = PROTO_VERSION_V2;
        c.len     = ACT_SIZE - 1;
        c.seq     = seq;
        c.mode    = 2; // VELOCITY
        c.tgt_q15 = vel_to_q15(v_joint_setpoint_rad_s);
        c.crc8    = crc8_sum(reinterpret_cast<const uint8_t*>(&c), ACT_SIZE - 1, 0x00);
    }

    // check incoming telemetry frame
    static bool validateTelemetryV2(const uint8_t* buf, size_t n){
        if (n != TEL2_SIZE) return false;
        const TelemetryFrameV2& f = *reinterpret_cast<const TelemetryFrameV2*>(buf);
        if (f.sof     != TEL_SOF)          return false;
        if (f.version != PROTO_VERSION_V2) return false;
        if (f.len     != TEL2_LEN)         return false;
        uint8_t calc  = crc8_sum(buf, TEL2_SIZE - 1, 0x00);
        return (calc == f.crc8);
    }

    // SLIP: send one byte with escaping
    static void slip_write_byte(HardwareSerial& U, uint8_t b){
        if (b == SLIP_END){ U.write(SLIP_ESC); U.write(SLIP_ESC_END); }
        else if (b == SLIP_ESC){ U.write(SLIP_ESC); U.write(SLIP_ESC_ESC); }
        else U.write(b);
    }

    // SLIP: send full frame
    static void slip_write_frame(HardwareSerial& U, const uint8_t* data, size_t n){
        U.write(SLIP_END);
        for (size_t i=0; i<n; i++) slip_write_byte(U, data[i]);
        U.write(SLIP_END);
    }

    // ---- per-link context (we have two links: L and R) ----
    struct LinkCtx {
        HardwareSerial* U;                 // actual UART
        Aquiles::Detection::Leg leg;       // leg this link controls
        SemaphoreHandle_t        ioMutex;  // protect TX if needed
        uint8_t                  seq;      // TX sequence

        // SLIP RX state
        static const size_t RX_BUF_MAX = 256;
        uint8_t  rx_buf[RX_BUF_MAX];
        size_t   rx_len;
        bool     rx_in_frame;
        bool     esc;

        LinkCtx(): U(nullptr),
                   leg(Aquiles::Detection::Leg::L),
                   ioMutex(nullptr),
                   seq(0),
                   rx_len(0),
                   rx_in_frame(false),
                   esc(false) {}
    };

    // one context per leg
    static LinkCtx CTX_L;
    static LinkCtx CTX_R;

    // hold-on-OC state
    static inline uint32_t now_ms(){ return (uint32_t)(esp_timer_get_time()/1000ULL); }
    static uint32_t hold_until_L_ms = 0;
    static uint32_t hold_until_R_ms = 0;

    // when a valid telemetry frame comes, we ingest it into Detection
    static void ingestTelemetryV2(const TelemetryFrameV2& f, Aquiles::Detection::Leg leg){
        // build a detection sample from the telemetry
        Aquiles::Detection::SampleIn s{};
        s.ts_us       = f.t_us;
        s.theta_rad   = q15_to_rad(f.enc_q15);
        s.accel_fwd_g = NAN;    // IMU data comes elsewhere
        s.imu_sat     = false;

        // push to detection
        Aquiles::Detection::pushSample(leg, s);

        // if OC flag is set, freeze for 200 ms and force target to current angle
        static constexpr uint16_t FLAG_OC = (1u<<0);
        if (f.flags & FLAG_OC){
            uint32_t t = now_ms();
            if (leg == Aquiles::Detection::Leg::L) hold_until_L_ms = t + 200;
            else                                   hold_until_R_ms = t + 200;
            Aquiles::Detection::setAngleTarget(leg, q15_to_rad(f.enc_q15));
        }
    }

    // SLIP RX byte feeding
    static void ctx_slip_feed(LinkCtx& C, uint8_t b, void (*onFrame)(uint8_t*, size_t)){
        // END marks the end of a frame
        if (b == SLIP_END){
            // we have a complete frame
            if (C.rx_in_frame && C.rx_len >= TEL2_SIZE){
                if (onFrame) onFrame(C.rx_buf, C.rx_len);
            }
            // reset state for next frame
            C.rx_len      = 0;
            C.rx_in_frame = true;
            C.esc         = false;
            return;
        }
        // ignore if we are not inside a frame yet
        if (!C.rx_in_frame) return;

        // handle escaped bytes
        if (C.esc){
            if      (b == SLIP_ESC_END) b = SLIP_END;
            else if (b == SLIP_ESC_ESC) b = SLIP_ESC;
            C.esc = false;
        } else if (b == SLIP_ESC){
            C.esc = true;
            return;
        }

        // store byte if buffer is not full
        if (C.rx_len < LinkCtx::RX_BUF_MAX) C.rx_buf[C.rx_len++] = b;
    }

    // frame handlers (one per leg)
    static void on_slip_frame_L(uint8_t* buf, size_t n){
        if (!validateTelemetryV2(buf, n)) return;
        const TelemetryFrameV2& f = *reinterpret_cast<const TelemetryFrameV2*>(buf);
        ingestTelemetryV2(f, Aquiles::Detection::Leg::L);
    }
    static void on_slip_frame_R(uint8_t* buf, size_t n){
        if (!validateTelemetryV2(buf, n)) return;
        const TelemetryFrameV2& f = *reinterpret_cast<const TelemetryFrameV2*>(buf);
        ingestTelemetryV2(f, Aquiles::Detection::Leg::R);
    }

    // TX task: sends actuation frames at 1 kHz
    static void vTxTask(void* pv){
        LinkCtx* C = static_cast<LinkCtx*>(pv);
        TickType_t last = xTaskGetTickCount();
        for(;;){
            // if E-STOP is active, stop transmitting
            if (Aquiles::Safety::triggered()){
                Serial.println("[UARTDual] E-STOP: stopping TX task.");
                vTaskDelete(nullptr);
            }

            // get current command from detection for this leg
            auto cmd = Aquiles::Detection::getCmd(C->leg);

            // apply hold if there was an OC event
            uint32_t t = now_ms();
            using Aquiles::Detection::Leg;
            if (C->leg == Leg::L && t < hold_until_L_ms) cmd.setpoint = 0.0f;
            if (C->leg == Leg::R && t < hold_until_R_ms) cmd.setpoint = 0.0f;

            // build the frame
            ActuationCmdV1 c{};
            uint8_t seq = ++C->seq;
            buildActCmd(c, seq, cmd.setpoint);

            // write with SLIP
            if (C->ioMutex) xSemaphoreTake(C->ioMutex, portMAX_DELAY);
            slip_write_frame(*C->U, reinterpret_cast<uint8_t*>(&c), ACT_SIZE);
            if (C->ioMutex) xSemaphoreGive(C->ioMutex);

            vTaskDelayUntil(&last, pdMS_TO_TICKS(1)); // 1 kHz
        }
    }

    // RX task: reads bytes from UART and feeds SLIP parser
    static void vRxTask(void* pv){
        LinkCtx* C = static_cast<LinkCtx*>(pv);
        auto cb = (C->leg == Aquiles::Detection::Leg::L) ? &on_slip_frame_L : &on_slip_frame_R;
        for(;;){
            while (C->U->available()){
                uint8_t b = (uint8_t)C->U->read();
                ctx_slip_feed(*C, b, cb);
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // public: configure both UARTs and contexts
    void init(){
        // Left link context
        CTX_L.U   = &U_L;
        CTX_L.leg = Aquiles::Detection::Leg::L;
        if (!CTX_L.ioMutex) CTX_L.ioMutex = xSemaphoreCreateMutex();

        // Right link context
        CTX_R.U   = &U_R;
        CTX_R.leg = Aquiles::Detection::Leg::R;
        if (!CTX_R.ioMutex) CTX_R.ioMutex = xSemaphoreCreateMutex();

        // start UARTs
        U_L.begin(UART_BAUD, SERIAL_8N1, L_RX, L_TX);
        U_R.begin(UART_BAUD, SERIAL_8N1, R_RX, R_TX);

        Serial.printf("[UARTDual] L: %u bps (TX=%d RX=%d)\n", (unsigned)UART_BAUD, L_TX, L_RX);
        Serial.printf("[UARTDual] R: %u bps (TX=%d RX=%d)\n", (unsigned)UART_BAUD, R_TX, R_RX);
    }

    // public: start TX/RX tasks for both links
    void startTasks(){
        // Left
        xTaskCreatePinnedToCore(vTxTask, "UartTX_L", 4096, &CTX_L, 5, nullptr, 1);
        xTaskCreatePinnedToCore(vRxTask, "UartRX_L", 4096, &CTX_L, 6, nullptr, 1);
        // Right
        xTaskCreatePinnedToCore(vTxTask, "UartTX_R", 4096, &CTX_R, 5, nullptr, 1);
        xTaskCreatePinnedToCore(vRxTask, "UartRX_R", 4096, &CTX_R, 6, nullptr, 1);
    }

} // namespace UARTDual



// =========================================================
// 4. BASIC TASKS (utility / example)
// =========================================================
namespace Tasks {

    // small init task: set up pins, then delete itself
    void Init(void* /*params*/) {
        pinMode(9, OUTPUT);
        vTaskDelete(nullptr);
    }

    // blink task to confirm the system is alive
    void Blink(void* /*params*/) {
        for(;;){
            digitalWrite(9, !digitalRead(9));
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

} // namespace Tasks



// =========================================================
// 5. SAFETY / E-STOP
// =========================================================
namespace Safety {

    // choose pin according to your board
    constexpr int PIN_ESTOP = 39;
    // shared flag, written in ISR
    static volatile bool g_estop = false;

    // ISR: fired on button press
    void IRAM_ATTR isr_estop(){ g_estop = true; }

    // public init: sets pin mode and ISR
    void init(){
        pinMode(PIN_ESTOP, INPUT_PULLUP);  // active on LOW
        attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), isr_estop, FALLING);
    }

    // check if stop was hit
    bool triggered(){ return g_estop; }

} // namespace Safety



// =========================================================
// 6. IMU: periodic read of two MPU6050s
// =========================================================
namespace IMU {

    // one IMU per leg
    static Adafruit_MPU6050 mpuL, mpuR;
    // I2C bus used
    static TwoWire* W = &Wire;
    // stored config
    static Cfg G{};
    // whether each IMU responded
    static bool hasL = false, hasR = false;

    // helper: pick accel axis
    static float take_axis(const sensors_event_t& a, int8_t axis){
        switch(axis){
            case 0: return a.acceleration.x;
            case 1: return a.acceleration.y;
            case 2: return a.acceleration.z;
        }
        return 0.0f;
    }

    // helper: pick gyro axis
    static float take_gyro(const sensors_event_t& g, int8_t axis){
        switch(axis){
            case 0: return g.gyro.x;
            case 1: return g.gyro.y;
            case 2: return g.gyro.z;
        }
        return 0.0f;
    }

    // init both IMUs on a given bus
    bool init(const Cfg& cfg, TwoWire* bus){
        G = cfg;
        if (bus) W = bus;

        // ESP32: you can set SDA/SCL and frequency here
        W->begin(8, 18, 400000);  // SDA=8, SCL=18, 400 kHz

        // try to start both IMUs
        hasL = mpuL.begin(G.addr_L, W);
        hasR = mpuR.begin(G.addr_R, W);

        // Configure filter/range if available
        if (hasL) {
            mpuL.setAccelerometerRange(MPU6050_RANGE_4_G);
            mpuL.setGyroRange(MPU6050_RANGE_500_DEG);
            mpuL.setFilterBandwidth(MPU6050_BAND_44_HZ);
        }
        if (hasR) {
            mpuR.setAccelerometerRange(MPU6050_RANGE_4_G);
            mpuR.setGyroRange(MPU6050_RANGE_500_DEG);
            mpuR.setFilterBandwidth(MPU6050_BAND_44_HZ);
        }

        Serial.printf("[IMU] L addr=0x%02X %s | R addr=0x%02X %s\n",
            G.addr_L, hasL?"OK":"FAIL", G.addr_R, hasR?"OK":"FAIL");

        // return true if at least one IMU is present
        return hasL || hasR;
    }

    // task that periodically reads both IMUs and feeds Detection
    static void vTaskIMU(void*){
        const TickType_t per = pdMS_TO_TICKS(G.period_ms);
        for(;;){
            sensors_event_t aL, gL, tL;
            sensors_event_t aR, gR, tR;
            uint32_t ts = (uint32_t)micros();

            // left IMU
            if (hasL){
                mpuL.getEvent(&aL, &gL, &tL);
                float a_fwd_ms2 = (float)G.fwd_sign_L * take_axis(aL, G.fwd_axis_L);
                float a_fwd_g   = a_fwd_ms2 / 9.80665f;

                Aquiles::Detection::SampleIn s{};
                s.ts_us       = ts;
                s.theta_rad   = NAN;        // we don't touch angle here
                s.accel_fwd_g = a_fwd_g;
                s.omega_imu   = take_gyro(gL, G.fwd_axis_L);
                // crude saturation heuristic
                s.imu_sat     = (fabsf(a_fwd_g) > 3.0f) ||
                                (fabsf(s.omega_imu) > (1000.0f*(float)M_PI/180.0f));
                Aquiles::Detection::pushSample(Aquiles::Detection::Leg::L, s);
            }

            // right IMU
            if (hasR){
                mpuR.getEvent(&aR, &gR, &tR);
                float a_fwd_ms2 = (float)G.fwd_sign_R * take_axis(aR, G.fwd_axis_R);
                float a_fwd_g   = a_fwd_ms2 / 9.80665f;

                Aquiles::Detection::SampleIn s{};
                s.ts_us       = ts;
                s.theta_rad   = NAN;
                s.accel_fwd_g = a_fwd_g;
                s.omega_imu   = take_gyro(gR, G.fwd_axis_R);
                s.imu_sat     = (fabsf(a_fwd_g) > 3.0f) ||
                                (fabsf(s.omega_imu) > (1000.0f*(float)M_PI/180.0f));
                Aquiles::Detection::pushSample(Aquiles::Detection::Leg::R, s);
            }

            vTaskDelay(per);
        }
    }

    void startTask(){
        xTaskCreatePinnedToCore(vTaskIMU, "IMU", 4096, nullptr, 3, nullptr, 0);
    }

} // namespace IMU



// =========================================================
// 7. POWER / BATTERY MONITOR
// =========================================================
namespace Power {

    // ADC pins for each tap (adjust to your board)
    static constexpr int ADC_C1_PIN = 1;
    static constexpr int ADC_C2_PIN = 2;
    static constexpr int ADC_C3_PIN = 3;
    static constexpr int ADC_C4_PIN = 4;

    // ADC params
    static constexpr float VREF    = 3.300f;
    static constexpr int   ADC_MAX = 4095;   // 12bit

    // Voltage dividers (top and bottom resistors)
    static constexpr float Rbot[4] = {2200.0f, 2200.0f, 2200.0f, 2200.0f};
    static constexpr float Rtop[4] = {1200.0f, 4300.0f, 8200.0f, 12000.0f};

    // Per-tap calibration (gain/offset)
    struct Cal { float gain; float offs; };
    static Cal CAL[4] = {
        {1.000f, 0.000f},
        {1.000f, 0.000f},
        {1.000f, 0.000f},
        {1.000f, 0.000f}
    };

    // Filtered state
    static float tap_V[4]  = {0,0,0,0};   // tap voltages (cumulative)
    static float cell_V[4] = {0,0,0,0};   // individual cell voltages
    static SemaphoreHandle_t mtx = nullptr;

    // Filter and period
    static constexpr float     EMA_A   = 0.30f;              // smoothing factor
    static constexpr TickType_t PERIOD = pdMS_TO_TICKS(200); // 5 Hz

    static inline float ema(float x, float y, float a){ return a*x + (1.0f-a)*y; }

    // read and convert one tap to actual volts (with divider and calibration)
    static float readTapVolt_raw(int pin, int idx){
        uint16_t adc = analogRead(pin);
        float vadc = (adc * VREF) / ADC_MAX;                      // MCU pin voltage
        float vtap = vadc * (Rtop[idx] + Rbot[idx]) / Rbot[idx];  // actual tap voltage
        return CAL[idx].gain * vtap + CAL[idx].offs;              // apply calibration
    }

    // from taps (cumulative) compute each cell
    static void compute_cells_from_taps(){
        cell_V[0] = tap_V[0];
        cell_V[1] = tap_V[1] - tap_V[0];
        cell_V[2] = tap_V[2] - tap_V[1];
        cell_V[3] = tap_V[3] - tap_V[2];
        // basic noise clamp
        for (int i=0;i<4;i++) if (cell_V[i] < 0.0f) cell_V[i] = 0.0f;
    }

    // task that keeps reading the battery taps and updating filtered values
    static void vTaskMon(void*){
        for(;;){
            float t1 = readTapVolt_raw(ADC_C1_PIN,0);
            float t2 = readTapVolt_raw(ADC_C2_PIN,1);
            float t3 = readTapVolt_raw(ADC_C3_PIN,2);
            float t4 = readTapVolt_raw(ADC_C4_PIN,3);

            if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
            tap_V[0] = ema(t1, tap_V[0], EMA_A);
            tap_V[1] = ema(t2, tap_V[1], EMA_A);
            tap_V[2] = ema(t3, tap_V[2], EMA_A);
            tap_V[3] = ema(t4, tap_V[3], EMA_A);

            compute_cells_from_taps();
            if (mtx) xSemaphoreGive(mtx);

            vTaskDelay(PERIOD);
        }
    }

    // public: start battery monitor
    void init(){
        analogReadResolution(12);
        if (!mtx) mtx = xSemaphoreCreateMutex();
        xTaskCreatePinnedToCore(vTaskMon, "BattMon", 3072, nullptr, 2, nullptr, 0);
        Serial.println("[Power] Cell monitor started.");
    }

    // public: fine-tune calibration
    void setCal(uint8_t tapIdx, float gain, float offs){
        if (tapIdx < 4) { CAL[tapIdx].gain = gain; CAL[tapIdx].offs = offs; }
    }

    // public: get individual cell voltage in volts
    float getCellV(uint8_t i){
        float v=0;
        if (i<4){
            if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
            v = cell_V[i];
            if (mtx) xSemaphoreGive(mtx);
        }
        return v;
    }

    // public: pack SOC estimate (simple linear example)
    float getPackSOC(){
        // take full pack voltage (tap 4)
        if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
        float vpack = tap_V[3];
        if (mtx) xSemaphoreGive(mtx);

        // map 12.0..16.8 → 0..1 (for 4S Li-ion example)
        float soc = (vpack - 12.0f) / (16.8f - 12.0f);
        if (soc < 0) soc = 0;
        if (soc > 1) soc = 1;
        return soc;
    }

    // public: SOC based on the weakest cell
    float getPackSOCmin(){
        float minv = 10.0f;
        if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
        for (int i=0;i<4;i++){
            if (cell_V[i] < minv) minv = cell_V[i];
        }
        if (mtx) xSemaphoreGive(mtx);

        // map 3.0..4.2 → 0..1
        float soc = (minv - 3.0f) / (4.2f - 3.0f);
        if (soc < 0) soc = 0;
        if (soc > 1) soc = 1;
        return soc;
    }

} // namespace Power

} // namespace Aquiles