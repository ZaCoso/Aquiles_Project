#include "AppTasks.h"

namespace Aquiles {
    namespace Detection {

      // ========= Estado interno =========
      static GlobalCfg CFG;

      static QueueHandle_t qInL = nullptr;   // cola de entrada (prof 1, overwrite)
      static QueueHandle_t qInR = nullptr;

      static LegSignals SIG_L, SIG_R;        // señales derivadas
      static CmdOut     CMD_L, CMD_R;        // salida final (suavizada)
      static float      sp_cur_L = 0.0f, sp_cur_R = 0.0f;

      // Hint bilateral (opcional)
      static bool contra_hint_L = true;  // “R en stance”
      static bool contra_hint_R = true;  // “L en stance”

      // ========= Helpers =========
      static inline uint32_t micros32(){ return (uint32_t)micros(); }
      static inline float unwrap(float th, float th_prev){
        float d = th - th_prev;
        while(d >  (float)M_PI) d -= 2.f*(float)M_PI;
        while(d < -(float)M_PI) d += 2.f*(float)M_PI;
        return th_prev + d;
      }
      static inline float slew(float target, float current, float max_delta){
        float d = target - current;
        if(d >  max_delta) d =  max_delta;
        if(d < -max_delta) d = -max_delta;
        return current + d;
      }
      static inline float clamp(float v, float lo, float hi){
        return (v<lo)?lo:(v>hi)?hi:v;
      }

      // ========= Intención & Planner (plantillas) =========
      static IntentOut detectIntent(const LegSignals& s_i,
                                    const LegSignals& s_contra,
                                    const IntentCfg& icfg,
                                    bool contra_stance_hint)
      {
        // Evidencias mínimas: velocidad angular y/o pulso inercial
        bool geom   = (fabsf(s_i.omega) >= icfg.omega_min);
        bool inert  = (fabsf(s_i.a_fwd_g) >= icfg.acc_min_g) || geom;
        bool contra = contra_stance_hint || (fabsf(s_contra.omega) < 0.2f);

        // La persistencia t_win/t_hold la podés implementar después con contadores.
        bool persist= true;

        IntentOut out;
        out.enable = (geom && inert && contra && persist);
        out.phase  = out.enable ? Phase::SWING : Phase::STANCE;
        return out;
      }

      static CmdOut planner(const IntentOut& I,
                            const LegSignals& s,
                            const LimitsCfg& lims,
                            float slew_per_tick,
                            float& sp_accum /*estado del smoother*/)
      {
        // Objetivo “ideal”
        float sp_target = 0.0f;
        Mode  mode      = Mode::TORQUE;

        if (I.enable && I.phase==Phase::SWING){
          // Ejemplo: torque proporcional a ω (ajustá la ganancia 0.5f)
          sp_target = clamp(0.5f * s.omega, -lims.I_max, lims.I_max);
        }

        // Ventana articular básica
        if (s.theta < lims.pos_min || s.theta > lims.pos_max) sp_target = 0.0f;

        // Suavizado (slew)
        sp_accum = slew(sp_target, sp_accum, slew_per_tick);

        CmdOut out;
        out.mode     = mode;
        out.setpoint = sp_accum;
        return out;
      }

      // ========= Tareas =========

      // Fusion: deriva ω a partir del encoder y mantiene señales (1 kHz)
      static void taskFusion(void*){
        float thL_prev = 0.0f, thR_prev = 0.0f;
        uint32_t tsL_prev = 0, tsR_prev = 0;

        TickType_t last = xTaskGetTickCount();
        for(;;){
          // LEFT
          SampleIn inL;
          if (xQueueReceive(qInL, &inL, 0)==pdTRUE){
            uint32_t ts = inL.ts_us ? inL.ts_us : micros32();
            float th = unwrap(inL.theta_rad, thL_prev);
            float dt = (tsL_prev==0) ? 0.001f : (float)(ts - tsL_prev)/1e6f;  // s
            if (dt <= 0) dt = 1e-3f;
            float w  = (th - thL_prev)/dt;

            SIG_L.theta   = th;
            SIG_L.omega   = w;
            SIG_L.a_fwd_g = inL.accel_fwd_g;
            SIG_L.coh_ok  = !inL.imu_sat;      // placeholder
            SIG_L.rx_us   = ts;

            thL_prev = th; tsL_prev = ts;
          }

          // RIGHT
          SampleIn inR;
          if (xQueueReceive(qInR, &inR, 0)==pdTRUE){
            uint32_t ts = inR.ts_us ? inR.ts_us : micros32();
            float th = unwrap(inR.theta_rad, thR_prev);
            float dt = (tsR_prev==0) ? 0.001f : (float)(ts - tsR_prev)/1e6f;
            if (dt <= 0) dt = 1e-3f;
            float w  = (th - thR_prev)/dt;

            SIG_R.theta   = th;
            SIG_R.omega   = w;
            SIG_R.a_fwd_g = inR.accel_fwd_g;
            SIG_R.coh_ok  = !inR.imu_sat;
            SIG_R.rx_us   = ts;

            thR_prev = th; tsR_prev = ts;
          }

          vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.fusion_period_ms));
        }
      }

      // Planner + Smoother (1 kHz)
      static void taskPlanner(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
          // Intención bilateral (hint evita doble-swing)
          IntentOut IL = detectIntent(SIG_L, SIG_R, CFG.intent, contra_hint_L);
          IntentOut IR = detectIntent(SIG_R, SIG_L, CFG.intent, contra_hint_R);

          // Planner por pierna
          CMD_L = planner(IL, SIG_L, CFG.limits, CFG.slew_per_tick, sp_cur_L);
          CMD_R = planner(IR, SIG_R, CFG.limits, CFG.slew_per_tick, sp_cur_R);

          vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.planner_period_ms));
        }
      }

      // Logger (50 Hz por defecto)
      static void taskLogger(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
          // Ejemplo de log:
          // Serial.printf("L th=%.2f w=%.2f | R th=%.2f w=%.2f | spL=%.2f spR=%.2f\n",
          //   SIG_L.theta, SIG_L.omega, SIG_R.theta, SIG_R.omega, CMD_L.setpoint, CMD_R.setpoint);
          vTaskDelayUntil(&last, pdMS_TO_TICKS(CFG.timing.logger_period_ms));
        }
      }

      // ========= API =========
      void begin(const GlobalCfg& cfg){
        CFG = cfg;

        // Colas de entrada (prof 1, overwrite)
        qInL = xQueueCreate(1, sizeof(SampleIn));
        qInR = xQueueCreate(1, sizeof(SampleIn));

        // Tareas internas (pinned a los cores como en tu estilo)
        xTaskCreatePinnedToCore(taskFusion,  "Fusion",  3072, nullptr, 4, nullptr, 1);
        xTaskCreatePinnedToCore(taskPlanner, "Planner", 4096, nullptr, 3, nullptr, 1);
        xTaskCreatePinnedToCore(taskLogger,  "Logger",  2048, nullptr, 1, nullptr, 0);
      }

      void pushSample(Leg leg, const SampleIn& s){
        if (leg==Leg::L)  xQueueOverwrite(qInL, &s);
        else              xQueueOverwrite(qInR, &s);
      }

      const LegSignals& getSignals(Leg leg){
        return (leg==Leg::L) ? SIG_L : SIG_R;
      }

      CmdOut getCmd(Leg leg){
        return (leg==Leg::L) ? CMD_L : CMD_R;
      }

      void setLimits(const LimitsCfg& lims){ CFG.limits = lims; }
      void setIntentCfg(const IntentCfg& icfg){ CFG.intent = icfg; }
      void setSlew(float slew_per_tick){ CFG.slew_per_tick = slew_per_tick; }

      void setContraStanceHint(Leg leg_in_swing, bool contralateral_is_in_stance){
        if (leg_in_swing==Leg::L) contra_hint_L = contralateral_is_in_stance;
        else                      contra_hint_R = contralateral_is_in_stance;
      }

    } // namespace Detection
    // ================= BLE =================
    namespace BLE {
        NimBLECharacteristic* pCharacteristic = nullptr;
        volatile bool deviceConnected = false;
        std::string lastValue = "0";

        void MyCallbacks::onWrite(NimBLECharacteristic* pChar) {
            std::string value = std::string(pChar->getValue().c_str());
            if (!value.empty()) {
                Serial.print("Recibido via BLE: ");
                Serial.println(value.c_str());
                lastValue = value;
            }
        }

        void MyServerCallbacks::onConnect(NimBLEServer* /*pServer*/) {
            deviceConnected = true;
            Serial.println("Dispositivo conectado");
        }

        void MyServerCallbacks::onDisconnect(NimBLEServer* /*pServer*/) {
            deviceConnected = false;
            Serial.println("Dispositivo desconectado");
        }

        void init() {
            NimBLEDevice::init(DEVICE_NAME);

            NimBLEServer* pServer = NimBLEDevice::createServer();
            pServer->setCallbacks(new MyServerCallbacks());

            NimBLEService* pService = pServer->createService(SERVICE_UUID);

            pCharacteristic = pService->createCharacteristic(
                CHARACTERISTIC_UUID,
                NIMBLE_PROPERTY::READ |
                NIMBLE_PROPERTY::WRITE |
                NIMBLE_PROPERTY::NOTIFY
            );

            pCharacteristic->setValue("0");
            pCharacteristic->setCallbacks(new MyCallbacks());
            pService->start();

            NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(SERVICE_UUID);
            pAdvertising->setScanResponse(true);
            pAdvertising->start();

            Serial.println("ESP32 BLE listo para conexiones");

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

        void enviarEstado() {
            static int bat = 0;
            bat = (bat + 1) % 101;
            String mensaje = "BAT:" + String(bat);

            if (pCharacteristic) {
                pCharacteristic->setValue(mensaje.c_str());
                pCharacteristic->notify();
            }

            Serial.print("Enviado: ");
            Serial.println(mensaje);
        }

        void TaskNotify(void* /*pv*/) {
            const TickType_t period = pdMS_TO_TICKS(3000);
            for (;;) {
                if (deviceConnected) {
                    enviarEstado();
                    Serial.println(lastValue.c_str());
                }
                vTaskDelay(period);
            }
        }
    }

    // ================= TAREAS BÁSICAS =================
    namespace Tasks {
        void Init(void* /*params*/) {
            pinMode(46, OUTPUT);
            vTaskDelete(nullptr);
        }

        void Blink(void* /*params*/) {
            for (;;) {
                digitalWrite(46, !digitalRead(46));
                Serial.printf("Rk.Test1\r\n");
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }

    // ================= SPI BUS =================
    namespace SPIBus {

        static SPISettings RP2040_SETTINGS(1000000, MSBFIRST, SPI_MODE0);
        static SPISettings DEV_B_SETTINGS(8000000, MSBFIRST, SPI_MODE3);

        static SemaphoreHandle_t spiMutex = nullptr;

        static inline void lock()   { xSemaphoreTake(spiMutex, portMAX_DELAY); }
        static inline void unlock() { xSemaphoreGive(spiMutex); }
        static inline uint8_t xfer(uint8_t b) { return SPI.transfer(b); }

        uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init) {
            uint8_t crc = init;
            for (size_t i = 0; i < len; ++i) {
                crc ^= data[i];
                for (uint8_t k = 0; k < 8; ++k)
                    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
            }
            return crc;
        }

        void init() {
            pinMode(CS_RP2040, OUTPUT); digitalWrite(CS_RP2040, HIGH);
            pinMode(CS_DEV_B,  OUTPUT); digitalWrite(CS_DEV_B,  HIGH);
            SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_RP2040);
            if (!spiMutex) spiMutex = xSemaphoreCreateMutex();
            Serial.println("SPI Bus listo (un bus, múltiples CS).");
        }

        bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                                uint8_t resp6[6], uint32_t timeout_ms)
        {
            uint8_t frame[9] = {
                0xAA, 6,
                (uint8_t)(v1 & 0xFF), (uint8_t)((v1 >> 8) & 0xFF),
                (uint8_t)(v2 & 0xFF), (uint8_t)((v2 >> 8) & 0xFF),
                (uint8_t)(v3 & 0xFF), (uint8_t)((v3 >> 8) & 0xFF),
                0
            };
            frame[8] = crc8_sum(&frame[2], 6);

            lock();
            SPI.beginTransaction(RP2040_SETTINGS);
            digitalWrite(CS_RP2040, LOW);

            for (auto b : frame) (void)xfer(b);

            uint32_t t0 = millis();
            size_t got = 0;
            while (got < 6) {
                resp6[got++] = xfer(0x00);
                if ((millis() - t0) > timeout_ms) {
                    digitalWrite(CS_RP2040, HIGH);
                    SPI.endTransaction();
                    unlock();
                    return false;
                }
            }

            digitalWrite(CS_RP2040, HIGH);
            SPI.endTransaction();
            unlock();
            return true;
        }

        bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n) {
            if (!n) return true;
            lock();
            SPI.beginTransaction(DEV_B_SETTINGS);
            digitalWrite(CS_DEV_B, LOW);

            for (size_t i = 0; i < n; ++i) {
                uint8_t t = tx ? tx[i] : 0x00;
                uint8_t r = xfer(t);
                if (rx) rx[i] = r;
            }

            digitalWrite(CS_DEV_B, HIGH);
            SPI.endTransaction();
            unlock();
            return true;
        }

        void parse_triplet(const uint8_t* p, int16_t& a, int16_t& b, int16_t& c) {
            a = (int16_t)(p[0] | (p[1] << 8));
            b = (int16_t)(p[2] | (p[3] << 8));
            c = (int16_t)(p[4] | (p[5] << 8));
        }

        void vSpiTask_RP2040(void* /*pv*/) {
            int16_t v1 = 100, v2 = -200, v3 = 300;
            uint8_t rx6[6];
            for (;;) {
                bool ok = rp2040_send_packet(v1, v2, v3, rx6, 20);
                if (ok) {
                    int16_t r1, r2, r3;
                    parse_triplet(rx6, r1, r2, r3);
                    Serial.printf("[RP2040] RESP: %d, %d, %d\r\n", r1, r2, r3);
                } else {
                    Serial.println("[RP2040] Timeout/FAIL");
                }
                v1 += 1; v2 -= 2; v3 += 3;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        void vSpiTask_DevB(void* /*pv*/) {
            uint8_t tx[3] = {0x9F, 0x00, 0x00};
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
    } // namespace SPIBus

} // namespace Aquiles