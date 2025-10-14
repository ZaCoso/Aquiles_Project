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

    namespace UARTBus {

      // =================== CONFIG UART ===================
      // Ajustá pines a tu ESP32-S3 si difieren
      static constexpr int UART_TX   = 17;
      static constexpr int UART_RX   = 18;
      static constexpr uint32_t UART_BAUD = 2000000; // 2 Mbps

      HardwareSerial& UART = Serial1;

      // =================== Protocolo (idéntico a SPI) ===================
      static constexpr uint8_t PROTO_VERSION = 0x01;
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
        uint8_t  rsv1, crc8;
      };
      struct TelemetryFrameV1 {
        uint8_t  sof, version, len, seq;
        uint32_t t_us;
        uint16_t flags, sample_hz;
        int16_t  enc_q15;
        int16_t  mpu_ax_q14, mpu_ay_q14, mpu_az_q14;
        int16_t  mpu_gx_q14, mpu_gy_q14, mpu_gz_q14;
        uint16_t err_code, frame_ctr;
        uint8_t  reserved[5], crc8;
      };
      #pragma pack(pop)

      // ===== Utilitarios compatibles =====
      uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init){
        uint8_t crc = init;
        for(size_t i=0;i<len;++i){
          crc ^= data[i];
          for(uint8_t k=0;k<8;++k)
            crc = (crc & 0x80) ? (uint8_t)((crc<<1) ^ 0x07) : (uint8_t)(crc<<1);
        }
        return crc;
      }
      float q15_to_rad(int16_t q){ return (float)q * 2.0f * (float)M_PI / 32768.0f; }
      float q14_to_g  (int16_t q){ return (float)q / 16384.0f; }
      int16_t amps_to_q15(float a){ return (int16_t)lrintf(a * 1024.0f); }

      inline int16_t vel_to_q15(float vel_rad_s){ return (int16_t)lrintf(vel_rad_s * 2048.0f); }
      inline float   q15_to_vel(int16_t q)      { return (float)q / 2048.0f; }

      // ===== SLIP TX helpers =====
      static void slip_write_byte(uint8_t b){
        if (b==SLIP_END){ UART.write(SLIP_ESC); UART.write(SLIP_ESC_END); }
        else if (b==SLIP_ESC){ UART.write(SLIP_ESC); UART.write(SLIP_ESC_ESC); }
        else UART.write(b);
      }
      static void slip_write_frame(const uint8_t* data, size_t n){
        UART.write(SLIP_END);
        for(size_t i=0;i<n;i++) slip_write_byte(data[i]);
        UART.write(SLIP_END);
      }

      // ===== Forward declarations (para orden correcto en C++11) =====
      typedef void (*SlipFrameHandler)(uint8_t*, size_t);
      static void slip_feed(uint8_t b, SlipFrameHandler onFrame);
      static void on_slip_frame(uint8_t* buf, size_t n);  // callback RX

      // ===== SLIP RX (única definición; NO dupliques esta función) =====
      static void slip_feed(uint8_t b, SlipFrameHandler onFrame){
        static constexpr size_t RX_BUF_MAX = 256;
        static uint8_t  rx_buf[RX_BUF_MAX];
        static size_t   rx_len = 0;
        static bool     rx_in_frame = false;
        static bool     esc = false;

        if (b == SLIP_END){
          if (rx_in_frame && rx_len >= TEL_SIZE){
            if (onFrame) onFrame(rx_buf, rx_len);
          }
          rx_len = 0;
          rx_in_frame = true;
          esc = false;
          return;
        }
        if (!rx_in_frame) return;

        if (esc){
          if (b==SLIP_ESC_END) b = SLIP_END;
          else if (b==SLIP_ESC_ESC) b = SLIP_ESC;
          esc = false;
        } else if (b == SLIP_ESC){
          esc = true;
          return;
        }

        if (rx_len < RX_BUF_MAX) rx_buf[rx_len++] = b;
      }

      // ===== Build & validate =====
      static void buildActCmd(ActuationCmdV1& c, uint8_t seq, float v_joint_setpoint_rad_s){
        memset(&c, 0, sizeof(c));
        c.sof = ACT_SOF;  c.version = PROTO_VERSION;  c.len = ACT_SIZE - 1;
        c.seq = seq;
        c.mode = 2; // VELOCITY
        c.tgt_q15 = vel_to_q15(v_joint_setpoint_rad_s);
        c.crc8 = crc8_sum(reinterpret_cast<const uint8_t*>(&c), ACT_SIZE - 1);
      }
      static bool validateTelemetry(const TelemetryFrameV1& f){
        if (f.sof!=TEL_SOF || f.version!=PROTO_VERSION || f.len!=TEL_LEN_FIELD) return false;
        uint8_t calc = crc8_sum(reinterpret_cast<const uint8_t*>(&f), TEL_SIZE - 1);
        return (calc == f.crc8);
      }

      // ===== OC hold y helpers =====
      static constexpr uint16_t FLAG_OC = (1u << 0);
      #include <esp_timer.h>
      static inline uint32_t now_ms(){ return (uint32_t)(esp_timer_get_time()/1000ULL); }
      static uint32_t hold_until_L_ms = 0;
      static uint32_t hold_until_R_ms = 0;

      static inline Aquiles::Detection::Leg leg_from_reserved(uint8_t id){
        return (id==0) ? Detection::Leg::L : Detection::Leg::R;
      }

      static void ingestTelemetry(const TelemetryFrameV1& f, Aquiles::Detection::Leg leg){
        Detection::SampleIn s{};
        s.ts_us       = f.t_us;
        s.theta_rad   = q15_to_rad(f.enc_q15);
        s.accel_fwd_g = q14_to_g(f.mpu_ax_q14);
        s.imu_sat     = (f.flags & (1u<<5)) != 0;
        Detection::pushSample(leg, s);

        if (f.flags & FLAG_OC){
          uint32_t t = now_ms();
          if (leg==Detection::Leg::L) hold_until_L_ms = t + 200;
          else                        hold_until_R_ms = t + 200;
          Aquiles::Detection::setAngleTarget(leg, q15_to_rad(f.enc_q15));
        }

        static uint32_t lastLog = 0;
        uint32_t now = millis();
        if (now - lastLog > 500){
          lastLog = now;
          Serial.printf("[UART] seq=%u frame=%u enc_q15=%d flags=0x%04X res=[%u,%u,%u,%u,%u]\n",
            f.seq, f.frame_ctr, f.enc_q15, f.flags,
            f.reserved[0], f.reserved[1], f.reserved[2], f.reserved[3], f.reserved[4]);
        }
      }

      // ===== Callback RX concreto (declarado arriba) =====
      static void on_slip_frame(uint8_t* buf, size_t n){
        if (n != TEL_SIZE) return;
        TelemetryFrameV1 f;
        memcpy(&f, buf, TEL_SIZE);
        if (!validateTelemetry(f)) return;
        auto leg = leg_from_reserved(f.reserved[0]); // 0=L, 1=R (set por RP2040)
        ingestTelemetry(f, leg);
      }

      // ===== Tareas =====
      static SemaphoreHandle_t ioMutex = nullptr;
      static uint8_t g_seq = 0;

      static void vUartTxTask(void*){
        TickType_t last = xTaskGetTickCount();
        for(;;){
          if (Aquiles::Safety::triggered()){
            Serial.println("[UART] E-STOP: deteniendo TX.");
            vTaskDelete(nullptr);
          }
          using Aquiles::Detection::Leg;
          auto cmdL = Aquiles::Detection::getCmd(Leg::L);
          auto cmdR = Aquiles::Detection::getCmd(Leg::R);

          uint32_t t = now_ms();
          if (t < hold_until_L_ms) cmdL.setpoint = 0.0f;
          if (t < hold_until_R_ms) cmdR.setpoint = 0.0f;

          ActuationCmdV1 cL{}, cR{};
          uint8_t seq = ++g_seq;
          buildActCmd(cL, seq, cmdL.setpoint);
          buildActCmd(cR, seq, cmdR.setpoint);

          if (ioMutex) xSemaphoreTake(ioMutex, portMAX_DELAY);
          slip_write_frame(reinterpret_cast<uint8_t*>(&cL), ACT_SIZE);
          slip_write_frame(reinterpret_cast<uint8_t*>(&cR), ACT_SIZE);
          if (ioMutex) xSemaphoreGive(ioMutex);

          vTaskDelayUntil(&last, pdMS_TO_TICKS(1)); // 1 kHz
        }
      }

      static void vUartRxTask(void*){
        for(;;){
          while (UART.available()){
            uint8_t b = (uint8_t)UART.read();
            slip_feed(b, &on_slip_frame); // puntero de función (C++11)
          }
          vTaskDelay(pdMS_TO_TICKS(1));
        }
      }

      // ===== API =====
      void init(){
        if (!ioMutex) ioMutex = xSemaphoreCreateMutex();
        UART.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
        Serial.printf("[UARTBus] Serial1 @ %u bps (TX=%d RX=%d)\n",
                      (unsigned)UART_BAUD, UART_TX, UART_RX);
      }
      void startTasks(){
        xTaskCreatePinnedToCore(vUartTxTask, "UartTX", 4096, nullptr, 5, nullptr, 1);
        xTaskCreatePinnedToCore(vUartRxTask, "UartRX", 4096, nullptr, 6, nullptr, 1);
      }

    } // namespace UARTBus

  
    namespace Safety {
        constexpr int PIN_ESTOP = 8;          // ajustá a tu botón
        static volatile bool g_estop = false;
        void IRAM_ATTR isr_estop(){ g_estop = true; }
        void init(){
          pinMode(PIN_ESTOP, INPUT_PULLDOWN);  // activo en HIGH
          attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), isr_estop, RISING);
        }
        inline bool triggered(){ return g_estop; }
        } // namespace Safety
      // ================= BATTERY / POWER =================
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