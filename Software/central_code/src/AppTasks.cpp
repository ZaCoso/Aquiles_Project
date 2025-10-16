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

      static float theta_ref_L = 0.0f;
      static float theta_ref_R = 0.0f;

      void setAngleTarget(Leg leg, float theta_ref_rad) {
        if (leg == Leg::L) theta_ref_L = theta_ref_rad;
        else               theta_ref_R = theta_ref_rad;
      }


      float getAngleTarget(Leg leg){
        return (leg == Leg::L) ? theta_ref_L : theta_ref_R;
      }

       // namespaces
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
      static CmdOut planner_angleP(const IntentOut& I,
                                  const LegSignals& s,
                                  const LimitsCfg& lims,
                                  const GlobalCfg& C,
                                  float& sp_accum /*estado del smoother*/)
      {
        float theta_ref = (&s==&SIG_L) ? theta_ref_L : theta_ref_R;  // truco si lo dejás así
        float err = theta_ref - s.theta;

        // Ventana articular: si estás fuera, clamp al borde (protección suave)
        if (s.theta < lims.pos_min)    err = lims.pos_min - s.theta;
        if (s.theta > lims.pos_max)    err = lims.pos_max - s.theta;

        // Deadband para evitar caza
        if (fabsf(err) < C.angle.deadband_rad) err = 0.0f;

        // P puro: velocidad articular deseada
        float v_joint = C.angle.kp_vel * err;
        v_joint = clamp(v_joint, -C.angle.vmax_joint, C.angle.vmax_joint);

        // Suavizado (slew) de la velocidad articular
        sp_accum = slew(v_joint, sp_accum, C.slew_per_tick);  // sp_accum ahora es v_joint_suavizada

        CmdOut out;
        out.mode     = Mode::VELOCITY;     // <--- importante: modo VELOCIDAD
        out.setpoint = sp_accum;           // <-- [rad/s] ARTICULAR
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
            // timestamp seguro
            uint32_t ts = inL.ts_us ? inL.ts_us : micros32();

            // Si viene un ángulo, actualizamos theta/omega
            if (!isnan(inL.theta_rad)){
              float th = unwrap(inL.theta_rad, thL_prev);
              float dt = (tsL_prev==0) ? 0.001f : (float)(ts - tsL_prev)/1e6f;
              if (dt <= 0) dt = 1e-3f;
              float w  = (th - thL_prev)/dt;

              SIG_L.theta = th;
              SIG_L.omega = w;
              thL_prev = th; tsL_prev = ts;
            }

            // Si viene aceleración, la actualizamos
            if (!isnan(inL.accel_fwd_g)) SIG_L.a_fwd_g = inL.accel_fwd_g;

            // Si viene flag de saturación IMU, actualizamos
            if (inL.imu_sat) SIG_L.coh_ok = false;

            SIG_L.rx_us = ts;
          }

          // RIGHT
          SampleIn inR;
          if (xQueueReceive(qInR, &inR, 0)==pdTRUE){
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

      // Planner + Smoother (1 kHz)
      static void taskPlanner(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
          // Intención bilateral (hint evita doble-swing)
          IntentOut IL = detectIntent(SIG_L, SIG_R, CFG.intent, contra_hint_L);
          IntentOut IR = detectIntent(SIG_R, SIG_L, CFG.intent, contra_hint_R);

          // Planner por pierna
          CMD_L = planner_angleP(IL, SIG_L, CFG.limits, CFG, sp_cur_L);
          CMD_R = planner_angleP(IR, SIG_R, CFG.limits, CFG, sp_cur_R);
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
        BLECharacteristic* pCharacteristic = nullptr;
        volatile bool deviceConnected = false;
        std::string lastValue = "0";
        // ===== Telemetría binaria BLE v1 =====
        #pragma pack(push,1)
        struct TelemetryBLEv1 {
          uint8_t  sof;       // 0xD1
          uint8_t  ver;       // 0x01
          uint8_t  len;       // sizeof(TelemetryBLEv1)-1
          uint8_t  seq;       // ++
          uint32_t t_ms;      // millis()
          int16_t  thL_q15;   // rad -> q15: x * 32768/(2π)
          int16_t  wL_q15;    // rad/s -> q15: x * 2048
          int16_t  aL_q14;    // g -> q14: x * 16384
          int16_t  thR_q15;
          int16_t  wR_q15;
          int16_t  aR_q14;
          uint16_t soc_q10;   // % -> q10: x * 1024
          uint16_t socMin_q10;
          uint16_t flags;     // bit0=OC (si lo usás), bit1=ESTOP
          uint8_t  rsv0;
          uint8_t  crc8;      // poly 0x07 sobre todo excepto crc
        };
        #pragma pack(pop)

        static inline int16_t _ble_rad_to_q15(float x){ return (int16_t)lrintf(x * (32768.0f/(2.0f*(float)M_PI))); }
        static inline int16_t _ble_vel_to_q15(float x){ return (int16_t)lrintf(x * 2048.0f); }
        static inline int16_t _ble_g_to_q14  (float x){ return (int16_t)lrintf(x * 16384.0f); }
        static inline uint16_t _ble_pct_to_q10(float x){ float p = (x<0?0:(x>100?100:x)); return (uint16_t)lrintf(p * 1024.0f); }

        static uint8_t _ble_crc8_07(const uint8_t* data, size_t n, uint8_t init=0x00){
          uint8_t c=init;
          for(size_t i=0;i<n;i++){ c^=data[i]; for(int k=0;k<8;k++) c=(c&0x80)?(uint8_t)((c<<1)^0x07):(uint8_t)(c<<1); }
          return c;
        }
        void MyCallbacks::onWrite(BLECharacteristic* pChar) {
            std::string value = std::string(pChar->getValue().c_str());
            if (!value.empty()) {
                Serial.print("Recibido via BLE: ");
                Serial.println(value.c_str());
                lastValue = value;
            }
        }

        void MyServerCallbacks::onConnect(BLEServer* /*pServer*/) {
            deviceConnected = true;
            Serial.println("Dispositivo conectado");
            BLEDevice::startAdvertising();
        }

        void MyServerCallbacks::onDisconnect(BLEServer* /*pServer*/) {
            deviceConnected = false;
            Serial.println("Dispositivo desconectado");
        }

        void init() {
            BLEDevice::init(DEVICE_NAME);

            BLEServer* pServer = BLEDevice::createServer();
            pServer->setCallbacks(new MyServerCallbacks());

            BLEService* pService = pServer->createService(SERVICE_UUID);

            pCharacteristic = pService->createCharacteristic(
                CHARACTERISTIC_UUID,
                BLECharacteristic::PROPERTY_READ   |
                BLECharacteristic::PROPERTY_NOTIFY |
                BLECharacteristic::PROPERTY_WRITE
            );

            pCharacteristic->setValue("0");
            pCharacteristic->setCallbacks(new MyCallbacks());
            pService->start();

            BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
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
            float pavg = Aquiles::Power::getPackSOC();
            char msg[96];
            snprintf(msg, sizeof(msg), "BAT:avg=%.1f",pavg);

            if (pCharacteristic) {
                pCharacteristic->setValue(msg);
                pCharacteristic->notify();
            }

            Serial.print("Enviado: ");
            Serial.println(msg);
        }
        void TaskNotify(void* /*pv*/) {
          // Intentar MTU alto para notificar frames largos de una sola vez
          BLEDevice::setMTU(185);

          uint8_t seq = 0;
          const TickType_t period = pdMS_TO_TICKS(50); // 20 Hz

          for (;;) {
            if (deviceConnected && pCharacteristic) {
              // Tomamos señales de la capa Detection
              auto sL = Aquiles::Detection::getSignals(Aquiles::Detection::Leg::L);
              auto sR = Aquiles::Detection::getSignals(Aquiles::Detection::Leg::R);

              TelemetryBLEv1 f{};
              f.sof  = 0xD1;
              f.ver  = 0x01;
              f.len  = sizeof(TelemetryBLEv1)-1;
              f.seq  = ++seq;
              f.t_ms = millis();

              f.thL_q15 = _ble_rad_to_q15(sL.theta);
              f.wL_q15  = _ble_vel_to_q15(sL.omega);
              f.aL_q14  = _ble_g_to_q14  (sL.a_fwd_g);

              f.thR_q15 = _ble_rad_to_q15(sR.theta);
              f.wR_q15  = _ble_vel_to_q15(sR.omega);
              f.aR_q14  = _ble_g_to_q14  (sR.a_fwd_g);

              float soc    = Aquiles::Power::getPackSOC();
              float socMin = Aquiles::Power::getPackSOCmin();
              f.soc_q10    = _ble_pct_to_q10(soc);
              f.socMin_q10 = _ble_pct_to_q10(socMin);

              // Flags
              f.flags = 0;
              if (Aquiles::Safety::triggered()) f.flags |= (1u<<1);
              // si querés propagar OC desde otro lado, OR bit0 acá

              f.crc8 = _ble_crc8_07(reinterpret_cast<const uint8_t*>(&f), sizeof(TelemetryBLEv1)-1, 0x00);

              // Notificar
              pCharacteristic->setValue(reinterpret_cast<uint8_t*>(&f), sizeof(f));
              pCharacteristic->notify();
            }
            vTaskDelay(period);
          }
        }
    }

    // ================= TAREAS BÁSICAS =================
    namespace Tasks {
        void Init(void* /*params*/) {
            pinMode(9, OUTPUT);
            vTaskDelete(nullptr);
        }

        void Blink(void* /*params*/) {
            for (;;) {
                digitalWrite(9, !digitalRead(9));
                Serial.printf("Rk.Test1\r\n");
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }

    namespace UARTDual {

    // =================== CONFIG GLOBAL ===================
    static constexpr uint32_t UART_BAUD = 2000000; // 2 Mbps

    // ---- Enlace L (Serial1) ----
    static constexpr int L_TX = 5;
    static constexpr int L_RX = 6;

    // ---- Enlace R (Serial2) ----
    static constexpr int R_TX = 7;
    static constexpr int R_RX = 15;

    HardwareSerial& U_L = Serial1;
    HardwareSerial& U_R = Serial2;

    // =================== Protocolo (idéntico a SPI) ===================
    static constexpr uint8_t PROTO_VERSION_V2 = 0x02; // telemetría nueva (20B
    static constexpr uint8_t ACT_SOF       = 0x5A;
    static constexpr uint8_t TEL_SOF       = 0xA5;
    static constexpr size_t  ACT_SIZE      = 16;
    static constexpr size_t  TEL_SIZE      = 36;
    static constexpr uint8_t TEL_LEN_FIELD = 35;

    // SLIP
    static constexpr uint8_t SLIP_END     = 0xC0;
    static constexpr uint8_t SLIP_ESC     = 0xDB;
    static constexpr uint8_t SLIP_ESC_END = 0xDC;
    static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

    #pragma pack(push,1)
    struct ActuationCmdV1 {
      uint8_t  sof, version, len, seq;
      uint8_t  mode, rsv0;
      int16_t  tgt_q15, kp_q15, kd_q15;
      uint16_t limits;
      uint8_t  rsv1, crc8; // rsv1 sin uso en modo dual-bus
    };

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

    static constexpr size_t TEL2_SIZE      = sizeof(TelemetryFrameV2); // 20
    static constexpr uint8_t TEL2_LEN      = TEL2_SIZE - 1;
        // ===== Utilitarios =====
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
    static float   q14_to_g  (int16_t q){ return (float)q / 16384.0f; }
    static int16_t vel_to_q15(float vel_rad_s){ return (int16_t)lrintf(vel_rad_s * 2048.0f); }

    static void buildActCmd(ActuationCmdV1& c, uint8_t seq, float v_joint_setpoint_rad_s){
      memset(&c, 0, sizeof(c));
      c.sof = ACT_SOF;  c.version = PROTO_VERSION_V2;  c.len = ACT_SIZE - 1;
      c.seq = seq;
      c.mode = 2; // VELOCITY
      c.tgt_q15 = vel_to_q15(v_joint_setpoint_rad_s);   
      c.crc8 = crc8_sum(reinterpret_cast<const uint8_t*>(&c), ACT_SIZE - 1, 0x00);
    }
    static bool validateTelemetryV2(const uint8_t* buf, size_t n){
      if (n != TEL2_SIZE) return false;
      const TelemetryFrameV2& f = *reinterpret_cast<const TelemetryFrameV2*>(buf);
      if (f.sof != TEL_SOF) return false;
      if (f.version != PROTO_VERSION_V2) return false;
      if (f.len != TEL2_LEN) return false;
      uint8_t calc = crc8_sum(buf, TEL2_SIZE - 1, 0x00);
      return (calc == f.crc8);
    }
    // ==== SLIP helpers ====
    static void slip_write_byte(HardwareSerial& U, uint8_t b){
      if (b==SLIP_END){ U.write(SLIP_ESC); U.write(SLIP_ESC_END); }
      else if (b==SLIP_ESC){ U.write(SLIP_ESC); U.write(SLIP_ESC_ESC); }
      else U.write(b);
    }
    static void slip_write_frame(HardwareSerial& U, const uint8_t* data, size_t n){
      U.write(SLIP_END);
      for(size_t i=0;i<n;i++) slip_write_byte(U, data[i]);
      U.write(SLIP_END);
    }

    typedef void (*SlipFrameHandler)(uint8_t*, size_t);

    static void slip_feed_byte(uint8_t b, SlipFrameHandler onFrame){
      // Un parser por tarea RX; lo haremos miembro de un “contexto”
      // Para permitir dos instancias, vamos a mover esto a un struct.
    }

    // ====== Contexto reutilizable por bus ======
    struct LinkCtx {
      HardwareSerial* U;
      Aquiles::Detection::Leg leg;     // L o R
      SemaphoreHandle_t ioMutex;
      uint8_t seq;

      // RX state (SLIP)
      static const size_t RX_BUF_MAX = 256;
      uint8_t  rx_buf[RX_BUF_MAX];
      size_t   rx_len;
      bool     rx_in_frame;
      bool     esc;

      LinkCtx(): U(nullptr), leg(Aquiles::Detection::Leg::L),
                ioMutex(nullptr), seq(0),
                rx_len(0), rx_in_frame(false), esc(false) {}
    };

    // Dos contextos: uno por enlace
    static LinkCtx CTX_L;
    static LinkCtx CTX_R;

    static void ctx_slip_feed(LinkCtx& C, uint8_t b, SlipFrameHandler onFrame){
      if (b == SLIP_END){
        if (C.rx_in_frame && C.rx_len >= TEL2_SIZE){
          if (onFrame) onFrame(C.rx_buf, C.rx_len);
        }
        C.rx_len = 0;
        C.rx_in_frame = true;
        C.esc = false;
        return;
      }
      if (!C.rx_in_frame) return;

      if (C.esc){
        if (b==SLIP_ESC_END) b = SLIP_END;
        else if (b==SLIP_ESC_ESC) b = SLIP_ESC;
        C.esc = false;
      } else if (b == SLIP_ESC){
        C.esc = true;
        return;
      }

      if (C.rx_len < LinkCtx::RX_BUF_MAX) C.rx_buf[C.rx_len++] = b;
    }

    // ===== OC hold =====
    static constexpr uint16_t FLAG_OC = (1u<<0);
    #include <esp_timer.h>
    static inline uint32_t now_ms(){ return (uint32_t)(esp_timer_get_time()/1000ULL); }
    static uint32_t hold_until_L_ms = 0;
    static uint32_t hold_until_R_ms = 0;

    static void ingestTelemetryV2(const TelemetryFrameV2& f, Aquiles::Detection::Leg leg){
      Aquiles::Detection::SampleIn s{};
      s.ts_us       = f.t_us;
      s.theta_rad   = q15_to_rad(f.enc_q15);
      s.accel_fwd_g = NAN;      // IMU viene por I2C
      s.imu_sat     = false;

      Aquiles::Detection::pushSample(leg, s);

      // OC → hold y clamp de referencia
      static constexpr uint16_t FLAG_OC = (1u<<0);
      if (f.flags & FLAG_OC){
        uint32_t t = now_ms();
        if (leg==Aquiles::Detection::Leg::L) hold_until_L_ms = t + 200;
        else                                  hold_until_R_ms = t + 200;
        Aquiles::Detection::setAngleTarget(leg, q15_to_rad(f.enc_q15));
      }
    }
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
    // ===== Tasks =====
    static void vTxTask(void* pv){
      LinkCtx* C = static_cast<LinkCtx*>(pv);
      TickType_t last = xTaskGetTickCount();
      for(;;){
        if (Aquiles::Safety::triggered()){
          Serial.println("[UARTDual] E-STOP: deteniendo TX.");
          vTaskDelete(nullptr);
        }

        using Aquiles::Detection::Leg;
        auto cmd = Aquiles::Detection::getCmd(C->leg);

        uint32_t t = now_ms();
        if (C->leg == Leg::L && t < hold_until_L_ms) cmd.setpoint = 0.0f;
        if (C->leg == Leg::R && t < hold_until_R_ms) cmd.setpoint = 0.0f;

        ActuationCmdV1 c{};
        uint8_t seq = ++C->seq;
        buildActCmd(c, seq, cmd.setpoint);

        if (C->ioMutex) xSemaphoreTake(C->ioMutex, portMAX_DELAY);
        slip_write_frame(*C->U, reinterpret_cast<uint8_t*>(&c), ACT_SIZE);
        if (C->ioMutex) xSemaphoreGive(C->ioMutex);

        vTaskDelayUntil(&last, pdMS_TO_TICKS(1)); // 1 kHz
      }
    }

    static void vRxTask(void* pv){
      LinkCtx* C = static_cast<LinkCtx*>(pv);
      SlipFrameHandler cb = (C->leg == Aquiles::Detection::Leg::L) ? &on_slip_frame_L : &on_slip_frame_R;
      for(;;){
        while (C->U->available()){
          uint8_t b = (uint8_t)C->U->read();
          ctx_slip_feed(*C, b, cb);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    // ===== API =====
    void init(){
      // Contexto Left
      CTX_L.U = &U_L;
      CTX_L.leg = Aquiles::Detection::Leg::L;
      if (!CTX_L.ioMutex) CTX_L.ioMutex = xSemaphoreCreateMutex();

      // Contexto Right
      CTX_R.U = &U_R;
      CTX_R.leg = Aquiles::Detection::Leg::R;
      if (!CTX_R.ioMutex) CTX_R.ioMutex = xSemaphoreCreateMutex();

      // Begin UARTs
      U_L.begin(UART_BAUD, SERIAL_8N1, L_RX, L_TX);
      U_R.begin(UART_BAUD, SERIAL_8N1, R_RX, R_TX);

      Serial.printf("[UARTDual] L: %u bps (TX=%d RX=%d)\n", (unsigned)UART_BAUD, L_TX, L_RX);
      Serial.printf("[UARTDual] R: %u bps (TX=%d RX=%d)\n", (unsigned)UART_BAUD, R_TX, R_RX);
    }

    void startTasks(){
      // L
      xTaskCreatePinnedToCore(vTxTask, "UartTX_L", 4096, &CTX_L, 5, nullptr, 1);
      xTaskCreatePinnedToCore(vRxTask, "UartRX_L", 4096, &CTX_L, 6, nullptr, 1);
      // R
      xTaskCreatePinnedToCore(vTxTask, "UartTX_R", 4096, &CTX_R, 5, nullptr, 1);
      xTaskCreatePinnedToCore(vRxTask, "UartRX_R", 4096, &CTX_R, 6, nullptr, 1);
    }

    } // namespace UARTDual
    namespace Safety {
        constexpr int PIN_ESTOP = 39;          // ajustá a tu botón
        static volatile bool g_estop = false;
        void IRAM_ATTR isr_estop(){ g_estop = true; }
        void init(){
          pinMode(PIN_ESTOP, INPUT_PULLUP);  // activo en HIGH
          attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), isr_estop, FALLING);
        }
        inline bool triggered(){ return g_estop; }
        } // namespace Safety
      // ================= BATTERY / POWER =================
    namespace IMU {

    static Adafruit_MPU6050 mpuL, mpuR;
    static TwoWire* W = &Wire;
    static Cfg G{};
    static bool hasL=false, hasR=false;

    static float take_axis(const sensors_event_t& a, int8_t axis){
      switch(axis){
        case 0: return a.acceleration.x; // m/s^2
        case 1: return a.acceleration.y;
        case 2: return a.acceleration.z;
        default: return 0.0f;
      }
    }
    static float take_gyro(const sensors_event_t& g, int8_t axis){
      switch(axis){
        case 0: return g.gyro.x;  // rad/s
        case 1: return g.gyro.y;
        case 2: return g.gyro.z;
        default: return 0.0f;
      }
    }

    bool init(const Cfg& cfg, TwoWire* bus){
      G = cfg;
      if (bus) W = bus;

      // Arrancar I2C (si no está ya)
      // En ESP32 puedes especificar pines si lo necesitás: W->begin(SDApin, SCLpin, freq);
      W->begin();

      // Intentar begin para ambos
      hasL = mpuL.begin(G.addr_L, W);
      hasR = mpuR.begin(G.addr_R, W);

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
      return hasL || hasR;
    }

    static void vTaskIMU(void*){
      const TickType_t per = pdMS_TO_TICKS(G.period_ms);
      for(;;){
        sensors_event_t aL, gL, tL;
        sensors_event_t aR, gR, tR;

        uint32_t ts = (uint32_t)micros();

        if (hasL){
          mpuL.getEvent(&aL, &gL, &tL);
          float a_fwd_ms2 = (float)G.fwd_sign_L * take_axis(aL, G.fwd_axis_L);
          float a_fwd_g   = a_fwd_ms2 / 9.80665f;

          Aquiles::Detection::SampleIn s{};
          s.ts_us       = ts;
          s.theta_rad   = NAN;          // no toco ángulo
          s.accel_fwd_g = a_fwd_g;      // sí actualizo accel
          // podés usar gyro si querés (omega_imu), ej: eje de flex/ext
          s.omega_imu   = take_gyro(gL, G.fwd_axis_L);
          // heurística saturación: |a|>3g o |gyro|>1000 dps
          s.imu_sat     = (fabsf(a_fwd_g)>3.0f) || (fabsf(s.omega_imu) > (1000.0f*(float)M_PI/180.0f));
          Aquiles::Detection::pushSample(Aquiles::Detection::Leg::L, s);
        }

        if (hasR){
          mpuR.getEvent(&aR, &gR, &tR);
          float a_fwd_ms2 = (float)G.fwd_sign_R * take_axis(aR, G.fwd_axis_R);
          float a_fwd_g   = a_fwd_ms2 / 9.80665f;

          Aquiles::Detection::SampleIn s{};
          s.ts_us       = ts;
          s.theta_rad   = NAN;
          s.accel_fwd_g = a_fwd_g;
          s.omega_imu   = take_gyro(gR, G.fwd_axis_R);
          s.imu_sat     = (fabsf(a_fwd_g)>3.0f) || (fabsf(s.omega_imu) > (1000.0f*(float)M_PI/180.0f));
          Aquiles::Detection::pushSample(Aquiles::Detection::Leg::R, s);
        }

        vTaskDelay(per);
      }
    }

    void startTask(){
      xTaskCreatePinnedToCore(vTaskIMU, "IMU", 4096, nullptr, 3, nullptr, 0);
    }

    } // namespace IMU
    namespace Power {

      // ---- PINS (ajustá a tu placa) ----
      static constexpr int ADC_C1_PIN = 1;   // tap celda 1 (V_C1)
      static constexpr int ADC_C2_PIN = 2;   // tap celda 2 (V_C2)
      static constexpr int ADC_C3_PIN = 3;   // tap celda 3 (V_C3)
      static constexpr int ADC_C4_PIN = 4;   // tap celda 4 (V_C4)

      // ---- ADC params ----
      static constexpr float VREF = 3.300f;   // referencia ADC efectiva (calibrar)
      static constexpr int   ADC_MAX = 4095;  // 12 bits

      // ---- Divisores (TU ESQUEMA) ----
      // Cada tap mide "pack parcial hasta esa celda" vs GND con un divisor distinto:
      // Vadc = Vtap * Rbottom/(Rtop+Rbottom)  => Vtap = Vadc * (Rtop+Rbottom)/Rbottom
      // Rbottom es 2.2k en los 4; Rtop es 1.2k, 4.3k, 8.2k, 12k respectivamente.
      static constexpr float Rbot[4] = {2200.0f, 2200.0f, 2200.0f, 2200.0f};
      static constexpr float Rtop[4] = {1200.0f, 4300.0f, 8200.0f,12000.0f};

      // ---- Calibración por tap (ganancia/offset) ----
      // Ajustá estos luego de medir con multímetro (gain ~1.0, offs ~0.0 V).
      struct Cal { float gain; float offs; };
      static Cal CAL[4] = {
        {1.000f, 0.000f}, {1.000f, 0.000f}, {1.000f, 0.000f}, {1.000f, 0.000f}
      };

      // ---- Estado filtrado ----
      static float tap_V[4]   = {0,0,0,0};   // voltaje real en cada tap (V_C1..V_C4)
      static float cell_V[4]  = {0,0,0,0};   // voltaje por celda
      static float cell_SOC[4]= {0,0,0,0};   // % por celda
      static float pack_SOC   = 0.0f;        // promedio
      static float pack_SOC_min = 0.0f;      // mínimo (conservador)
      static SemaphoreHandle_t mtx = nullptr;

      // ---- LUT OCV → % (temperatura ambiente, reposo/carga liviana) ----
      struct Pt { float v, soc; };
      static const Pt LUT[] = {
        {4.20f,100},{4.15f,95},{4.10f,90},{4.05f,85},{4.00f,80},
        {3.95f,70},{3.90f,60},{3.85f,50},{3.80f,40},{3.75f,30},
        {3.70f,20},{3.65f,12},{3.60f,8},{3.55f,5},{3.50f,3},
        {3.45f,2},{3.40f,1},{3.30f,0}
      };
      static constexpr int LUTN = sizeof(LUT)/sizeof(LUT[0]);

      static inline float soc_from_v(float v){
        if (v >= LUT[0].v) return 100.0f;
        if (v <= LUT[LUTN-1].v) return 0.0f;
        for (int i=0;i<LUTN-1;i++){
          if (v <= LUT[i].v && v >= LUT[i+1].v){
            float t = (v - LUT[i+1].v) / (LUT[i].v - LUT[i+1].v);
            return LUT[i+1].soc + t*(LUT[i].soc - LUT[i+1].soc);
          }
        }
        return 0.0f;
      }

      static inline float ema(float x, float y, float a){ return a*x + (1.0f-a)*y; }

      static float readTapVolt_raw(int pin, int idx){
        uint16_t adc = analogRead(pin);
        float vadc = (adc * VREF) / ADC_MAX;                           // V en el pin MCU
        float vtap = vadc * (Rtop[idx] + Rbot[idx]) / Rbot[idx];       // V real en el tap
        // Calibración por tap:
        return CAL[idx].gain * vtap + CAL[idx].offs;
      }

      static void compute_cells_from_taps(){
        // c1 = tap1
        cell_V[0] = tap_V[0];
        // c2 = tap2 - tap1
        cell_V[1] = tap_V[1] - tap_V[0];
        // c3 = tap3 - tap2
        cell_V[2] = tap_V[2] - tap_V[1];
        // c4 = tap4 - tap3
        cell_V[3] = tap_V[3] - tap_V[2];

        // Clamp por seguridad (evita negativos por ruido)
        for (int i=0;i<4;i++){
          if (cell_V[i] < 0.0f) cell_V[i] = 0.0f;
        }
      }

      static void update_soc(){
        float smin = 100.0f, ssum = 0.0f;
        for (int i=0;i<4;i++){
          cell_SOC[i] = soc_from_v(cell_V[i]);
          ssum += cell_SOC[i];
          if (cell_SOC[i] < smin) smin = cell_SOC[i];
        }
        pack_SOC = ssum / 4.0f;
        pack_SOC_min = smin;
      }

      // ===== Tarea de muestreo/filtrado =====
      static void vTaskMon(void*){
        const TickType_t period = pdMS_TO_TICKS(200);  // 5 Hz
        for(;;){
          float t1 = readTapVolt_raw(ADC_C1_PIN,0);
          float t2 = readTapVolt_raw(ADC_C2_PIN,1);
          float t3 = readTapVolt_raw(ADC_C3_PIN,2);
          float t4 = readTapVolt_raw(ADC_C4_PIN,3);

          // EMA suave (a=0.3)
          if (mtx) xSemaphoreTake(mtx, portMAX_DELAY);
          tap_V[0] = ema(t1, tap_V[0], 0.30f);
          tap_V[1] = ema(t2, tap_V[1], 0.30f);
          tap_V[2] = ema(t3, tap_V[2], 0.30f);
          tap_V[3] = ema(t4, tap_V[3], 0.30f);

          compute_cells_from_taps();
          update_soc();
          if (mtx) xSemaphoreGive(mtx);

          vTaskDelay(period);
        }
      }

      // ===== API =====
      void init(){
        analogReadResolution(12);
        // (opcional) analogSetAttenuation(ADC_11db);
        if (!mtx) mtx = xSemaphoreCreateMutex();
        xTaskCreatePinnedToCore(vTaskMon, "BattMon", 3072, nullptr, 2, nullptr, 0);
        Serial.println("[Power] Monitor de batería iniciado.");
      }

      // Seteo de calibración (ganancia/offset por tap)
      void setCal(uint8_t tapIdx, float gain, float offs){
        if (tapIdx<4) { CAL[tapIdx].gain = gain; CAL[tapIdx].offs = offs; }
      }

      // Getters thread-safe
      float getCellV(uint8_t i){ float v=0; if (i<4){ if(mtx) xSemaphoreTake(mtx,portMAX_DELAY); v=cell_V[i]; if(mtx) xSemaphoreGive(mtx);} return v; }
      float getCellSOC(uint8_t i){ float s=0; if (i<4){ if(mtx) xSemaphoreTake(mtx,portMAX_DELAY); s=cell_SOC[i]; if(mtx) xSemaphoreGive(mtx);} return s; }
      float getPackSOC(){ float s; if(mtx) xSemaphoreTake(mtx,portMAX_DELAY); s=pack_SOC; if(mtx) xSemaphoreGive(mtx); return s; }
      float getPackSOCmin(){ float s; if(mtx) xSemaphoreTake(mtx,portMAX_DELAY); s=pack_SOC_min; if(mtx) xSemaphoreGive(mtx); return s; }

    } // namespace Power
} // namespace Aquiles