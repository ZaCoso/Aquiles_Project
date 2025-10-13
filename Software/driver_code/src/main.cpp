#include <Arduino.h>
#include <Wire.h>
#define Wire Wire1  
#include <AS5600.h>

#include <Adafruit_MPU6050.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Sensor.h>
#include <SimpleFOC.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"
#include "hardware/adc.h"   

#include <vector>
#include <cstring>
#include <cmath>

#define EN1 0
#define EN2 4
#define EN3 2  
#define LED_PIN   16    // RP2040 Zero (Waveshare) suele ser 16
#define LED_COUNT 1

// ---- Declaración GLOBAL (fuera de cualquier función) ----
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

AS5600 encoder_i2c;    

Adafruit_MPU6050 mpu;

// --- Definición de hardware ---
BLDCMotor motor = BLDCMotor(7);                  // Motor con 7 pares de polos
BLDCDriver3PWM driver = BLDCDriver3PWM(1, 5, 3); // Pines PWM para el driver

float encoder_5600();   // devuelve ángulo del AS5600 en radianes (0..2π)
std::vector<float> readMPU6050();  
// Flag en Telemetry.flags para avisar sobrecorriente al master
static constexpr uint16_t FLAG_OC = (1u << 0);
namespace DriverSPI {
    static volatile uint32_t g_last_cmd_us = 0;
    // ===================== Config y protocolo =====================
    // ---- E-STOP por protocolo + modos ----
    enum : uint8_t { MODE_OFF=0, MODE_TORQUE=1, MODE_VELOCITY=2, MODE_POSITION=3 };
    static volatile bool g_estop_latched = false;
    struct Pins {
      uint8_t sck  = 10;
      uint8_t mosi = 11;
      uint8_t miso = 12;
      uint8_t cs   = 13;
    };

    static Pins PIN;  // editable antes de begin()

    static constexpr uint8_t  PROTO_VERSION  = 0x01;
    static constexpr uint8_t  ACT_SOF        = 0x5A;
    static constexpr uint8_t  TEL_SOF        = 0xA5;
    static constexpr size_t   ACT_SIZE       = 16;
    static constexpr size_t   TEL_SIZE       = 36;
    static constexpr uint8_t  TEL_LEN_FIELD  = 35;

    // Flag de sobrecorriente para avisar al master
    static constexpr uint16_t FLAG_OC = (1u << 0);

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
    // ===================== Estado interno =====================
    static repeating_timer_t g_tim;
    static volatile TelemetryFrameV1 g_tx_buf[2];
    static volatile uint8_t  g_idx_wr = 0;
    static volatile uint8_t  g_idx_rd = 1;
    static volatile uint32_t g_frame_ctr = 0;
    static volatile bool g_xfer_pending = false;
    static volatile uint8_t  g_seq       = 0;
    static volatile bool     g_started   = false;
    static volatile ActuationCmdV1 g_last_cmd{};

    // ===================== Estado interno =====================
    // --- Snapshot de sensores para usar en fillTelemetry() ---
    struct SensorSnap {
      float enc_rad;                 // 0..2π
      float ax_g, ay_g, az_g;
      float gx_rads, gy_rads, gz_rads;
      float iA, iB, iC;              // corrientes por fase [A]
      float i_peak;                  // max(|Ia|,|Ib|,|Ic|)
      uint8_t phase_peak;            // 0=A, 1=B, 2=C
      bool overcurrent;              // latch de OC
      uint32_t t_us;
    };
    static volatile SensorSnap g_snap = {};
    static volatile uint32_t   g_oc_hold_until_ms = 0;

    // ===================== Utils =====================
    static inline uint8_t crc8_0x07(const uint8_t* d, size_t n, uint8_t init = 0x00) {
      uint8_t c = init;
      for (size_t i=0;i<n;i++){ c ^= d[i]; for (uint8_t k=0;k<8;k++) c = (c & 0x80) ? (uint8_t)((c<<1) ^ 0x07) : (uint8_t)(c<<1); }
      return c;
    }

    static inline uint32_t micros32() { return (uint32_t) to_us_since_boot(get_absolute_time()); }
    static inline int16_t rad_to_q15(float rad) { return  (int16_t)((rad / (2.0f * (float)M_PI)) * 32768.0f);}
    static inline int16_t g_to_q14(float g)     { return (int16_t)(g * 16384.0f); }
    static inline int16_t gyro_to_q15(float rad_per_s, float max_rad_per_s) { return (int16_t)((rad_per_s / max_rad_per_s) * 32768.0f); }
    // ===================== Sensores (stub de ejemplo) =====================
    // ====== Calibración Corriente ACS712-05B ======
    static constexpr float VREF_V      = 3.3f;       // tensión que ve el ADC (tras divisor)
    static constexpr float ADC_COUNTS  = 4095.0f;
    // Si hay divisor: KDIV = V_adc/V_sensor. SIN divisor = 1.0 (¡cuidado 5V!)
    static constexpr float KDIV        = 1.0f;       // <--- AJUSTÁ si usás divisor
    static constexpr float SENS_SENSOR_VperA = 0.185f;                    // 05B
    static constexpr float SENS_ADC_VperA    = SENS_SENSOR_VperA * KDIV;  // lo que ve el ADC

    struct PhaseCal {
      float gain_VperA;   // V/A que ve el ADC
      float offset_V;     // V de offset en el ADC (se mide en autocal)
      int   adc_ch;       // 0,1,2 => ADC0/1/2 => GPIO26/27/28
    };
    static PhaseCal CAL_A{ SENS_ADC_VperA, VREF_V*0.5f, 0 };
    static PhaseCal CAL_B{ SENS_ADC_VperA, VREF_V*0.5f, 1 };
    static PhaseCal CAL_C{ SENS_ADC_VperA, VREF_V*0.5f, 2 };

    static float autocal_offset_V(int adc_ch, int samples=256) {
      float acc = 0;
      adc_select_input(adc_ch);
      for (int i=0;i<samples;i++){
        uint16_t raw = adc_read();
        acc += (raw * VREF_V) / ADC_COUNTS;
        sleep_us(50);
      }
      return acc / samples;
    }
    static inline float adc_to_V(int adc_ch) {
      adc_select_input(adc_ch);
      uint16_t raw = adc_read();
      return (raw * VREF_V) / ADC_COUNTS;
    }
    static inline float volts_to_A(float v_meas, const PhaseCal& c) {
      // Bipolar con offset ~Vref/2: I = (Vadc - Voffset_adc) / SENS_ADC_VperA
      return (v_meas - c.offset_V) / c.gain_VperA;
    }
    static inline void read_currents_3ph(float& Ia, float& Ib, float& Ic) {
      float vA = adc_to_V(CAL_A.adc_ch);
      float vB = adc_to_V(CAL_B.adc_ch);
      float vC = adc_to_V(CAL_C.adc_ch);
      Ia = volts_to_A(vA, CAL_A);
      Ib = volts_to_A(vB, CAL_B);
      Ic = volts_to_A(vC, CAL_C);
    }

    // ====== Sensores sin prints (para snapshot desde loop) ======
    static inline float read_encoder_rad_noprint() {
      uint16_t raw = encoder_i2c.readAngle(); // 0..4095
      return (raw * (2.0f * (float)M_PI)) / 4096.0f;
    }
    static inline void sample_mpu(float& ax_g, float& ay_g, float& az_g,
                                  float& gx, float& gy, float& gz) {
      sensors_event_t a, g, t;
      mpu.getEvent(&a,&g,&t);
      const float g0 = 9.80665f;
      ax_g = a.acceleration.x / g0;
      ay_g = a.acceleration.y / g0;
      az_g = a.acceleration.z / g0;
      gx   = g.gyro.x;
      gy   = g.gyro.y;
      gz   = g.gyro.z;
    }
    static void fillTelemetry(TelemetryFrameV1& f) {
      SensorSnap s{};
      uint32_t irq = save_and_disable_interrupts();
      memcpy(&s, (const void*)&g_snap, sizeof(SensorSnap));  
      restore_interrupts(irq);

      f.sof       = TEL_SOF;
      f.version   = PROTO_VERSION;
      f.len       = TEL_LEN_FIELD;
      f.seq       = g_seq;
      f.t_us      = micros32();
      f.flags     = 0;
      if (s.overcurrent) f.flags |= FLAG_OC;
      f.sample_hz = 1000;                 // 1 kHz
      f.enc_q15   = rad_to_q15(encoder_5600());
      // auto mpu_data = readMPU6050();  // actualizar datos del MPU6050
      // // IMU demo (dejar en 0/1g)
      // f.mpu_ax_q14 = g_to_q14(mpu_data[0]);
      // f.mpu_ay_q14 = g_to_q14(mpu_data[1]);
      // f.mpu_az_q14 = g_to_q14(mpu_data[2]);
      // f.mpu_gx_q14 = gyro_to_q15(mpu_data[3], 4.3633f);
      // f.mpu_gy_q14 = gyro_to_q15(mpu_data[4], 4.3633f);
      // f.mpu_gz_q14 = gyro_to_q15(mpu_data[5], 4.3633f);
      f.mpu_ax_q14 = 0;
      f.mpu_ay_q14 = 0;
      f.mpu_az_q14 = 0;
      f.mpu_gx_q14 = 0;
      f.mpu_gy_q14 = 0;
      f.mpu_gz_q14 = 0;

      f.err_code  = 0;
      f.frame_ctr = (uint16_t)(g_frame_ctr & 0xFFFF);

      // ---- Mapear I_peak y phase_peak compactos en reserved ----
      // reserved[0..1] = I_peak_q15 (A * 1024, signado)
      int16_t ipeak_q15 = (int16_t)lrintf(s.i_peak * 1024.0f);
      f.reserved[0] = (uint8_t)(ipeak_q15 & 0xFF);
      f.reserved[1] = (uint8_t)((ipeak_q15 >> 8) & 0xFF);
      // reserved[2] = phase_peak (0..2) y bit7=OC activo
      f.reserved[2] = (uint8_t)((s.overcurrent ? 0x80 : 0x00) | (s.phase_peak & 0x03));
      f.reserved[3] = 0;
      f.reserved[4] = 0;
      f.crc8      = crc8_0x07(reinterpret_cast<const uint8_t*>(&f), TEL_SIZE-1, 0x00);
    }

    // ===================== Timer 1 kHz: cerrar frame + swap =====================
    static bool timer_cb(repeating_timer_t*) {
      TelemetryFrameV1 w{};
      fillTelemetry(w);

      // copiar al buffer de escritura (volatile) y swappear
      uint32_t irq = save_and_disable_interrupts();
      memcpy((void*)&g_tx_buf[g_idx_wr], &w, sizeof(TelemetryFrameV1));
      uint8_t old_rd = g_idx_rd;
      g_idx_rd = g_idx_wr;
      g_idx_wr = old_rd;
      g_frame_ctr++;
      g_seq++;
      restore_interrupts(irq);

      return true;
    }

    // Forward decls (helpers implemented below)
    static inline bool rx_byte_timeout(uint8_t& b, uint32_t timeout_us);
    static inline bool tx_byte_timeout(uint8_t b, uint32_t timeout_us);

    // ===================== Transacción SPI: RX 16B + TX 36B =====================
    static bool serve_transaction_safe(uint32_t timeout_us_per_byte = 200) {
      if (gpio_get(PIN.cs)) return false;                   // CS alto → nada que hacer

      // 1) RX comando 16B con timeout
      ActuationCmdV1 cmd{};
      for (int i=0; i<(int)ACT_SIZE; ++i) {
        uint8_t b;
        if (!rx_byte_timeout(b, timeout_us_per_byte)) return false;  // aborta limpio
        reinterpret_cast<uint8_t*>(&cmd)[i] = b;
      }

      // Validación
      bool ok = (cmd.sof==ACT_SOF && cmd.version==PROTO_VERSION && cmd.len==(ACT_SIZE-1));
      uint8_t ccrc = crc8_0x07(reinterpret_cast<const uint8_t*>(&cmd), ACT_SIZE-1, 0x00);
      ok = ok && (ccrc == cmd.crc8);
      if (ok) {
        memcpy((void*)&g_last_cmd, &cmd, sizeof(ActuationCmdV1));
        g_last_cmd_us = micros32();
          if (cmd.mode == MODE_OFF) {
              g_estop_latched = true;
              }
      }

      // 2) Snapshot del último frame
      TelemetryFrameV1 snap{};
      {
        uint32_t irq = save_and_disable_interrupts();
        memcpy(&snap, (const void*)&g_tx_buf[g_idx_rd], sizeof(TelemetryFrameV1));
        restore_interrupts(irq);
      }

      // 3) TX 36B con timeout
      for (int i=0; i<(int)TEL_SIZE; ++i) {
        uint8_t b = reinterpret_cast<uint8_t*>(&snap)[i];
        if (!tx_byte_timeout(b, timeout_us_per_byte)) return false;
      }

      return true;
    }
    static inline bool rx_byte_timeout(uint8_t& b, uint32_t timeout_us){
      uint32_t t0 = micros32();
      while (!spi_is_readable(spi1)) {
        if (gpio_get(PIN.cs)) return false;                 // CS subió → abortar
        if ((micros32() - t0) > timeout_us) return false;   // sin reloj → abortar
      }
      b = (uint8_t)spi_get_hw(spi1)->dr;                    // leer un byte de RX FIFO
      return true;
    }

    static inline bool tx_byte_timeout(uint8_t b, uint32_t timeout_us){
      uint32_t t0 = micros32();
      while (!spi_is_writable(spi1)) {
        if (gpio_get(PIN.cs)) return false;
        if ((micros32() - t0) > timeout_us) return false;
      }
      spi_get_hw(spi1)->dr = b;                             // poner un byte en TX FIFO
      return true;
    }

    // ===================== IRQ por CS (flanco ↓ = 1 transacción) =================
    // Firma correcta para Philhower/Pico SDK:
    static void cs_isr(uint gpio, uint32_t events) {
      (void)events;
      if (gpio == PIN.cs && !gpio_get(PIN.cs)) {
        g_xfer_pending = true;  
      }
    }
    inline bool copyLastCmd(ActuationCmdV1& out, uint8_t& seq_out, uint32_t& age_us_out) {
      uint32_t irq = save_and_disable_interrupts();
      ActuationCmdV1 tmp;
      memcpy(&tmp, (const void*)&g_last_cmd, sizeof(ActuationCmdV1));
      uint8_t  sq  = g_seq;
      uint32_t age = micros32() - g_last_cmd_us;
      restore_interrupts(irq);
      out = tmp; seq_out = sq; age_us_out = age;

      bool ok = (out.sof==ACT_SOF && out.version==PROTO_VERSION && out.len==(ACT_SIZE-1));
      return ok;
    }
    // ===================== API mínimas =====================
    static bool g_inited = false;

    void setPins(const Pins& p) { PIN = p; }

    void begin() {
      if (g_inited) return;
      g_inited = true;

      // Pines SPI0
      gpio_set_function(PIN.sck,  GPIO_FUNC_SPI);
      gpio_set_function(PIN.mosi, GPIO_FUNC_SPI);
      gpio_set_function(PIN.miso, GPIO_FUNC_SPI);

      // CS entrada con pull-up (lo tira a LOW el master)
      gpio_init(PIN.cs); gpio_set_dir(PIN.cs, GPIO_IN); gpio_pull_up(PIN.cs);

      // DRDY opcional

      // en begin()
      spi_init(spi1, 1000*1000);
      spi_set_slave(spi1, true);
      spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

      // Registrar callback de GPIO con la firma correcta
      gpio_set_irq_enabled_with_callback(PIN.cs, GPIO_IRQ_EDGE_FALL, true, cs_isr);

      // ADC para corrientes (A/B/C en ADC0/1/2 → GPIO26/27/28)
      adc_init();
      adc_gpio_init(26);
      adc_gpio_init(27);
      adc_gpio_init(28);

      // Buffers iniciales
      TelemetryFrameV1 z{}; fillTelemetry(z);
      memcpy((void*)&g_tx_buf[0], &z, sizeof(TelemetryFrameV1));
      memcpy((void*)&g_tx_buf[1], &z, sizeof(TelemetryFrameV1));

      // Timer 1 kHz
      add_repeating_timer_us(-1000, timer_cb, nullptr, &g_tim);

      Serial.println("[DriverSPI] listo (SPI1 slave, 1 kHz).");
    }

    void end() {
      cancel_repeating_timer(&g_tim);
      gpio_set_irq_enabled(PIN.cs, GPIO_IRQ_EDGE_FALL, false);
      g_inited = false;
    }

    // Helpers opcionales “públicos”
    static inline int16_t amps_to_q15(float a){ return (int16_t)lrintf(a * 1024.0f); }

} // namespace DriverSPI
// --- Función para inicializar SimpleFOC ---
// ====== Open-loop: escalas y seguridad ======
static constexpr float   GEAR_RATIO              = 51.0f;     // 1:51
static constexpr float   MAX_VEL_RAD_S_MOTOR     = 100.0f;    // límite [rad/s] eje motor
static constexpr float   RAMP_RAD_S2             = 800.0f;    // rampa [rad/s^2]
static constexpr float   DEADBAND_RAD_S          = 0.5f;      // zona muerta cerca de 0
static constexpr uint32_t FAILSAFE_MS            = 50;        // si no hay cmd fresco → 0
// true: el ESP32 manda vel de SALIDA (post-reductora). false: manda vel de MOTOR.
static constexpr bool    COMMAND_IS_OUTPUT_SIDE  = true;

// Estado para rampa y dt
static float     g_target_vel_motor = 0.0f;   // [rad/s] objetivo suave
static uint32_t  g_last_tick_us     = 0;

static inline float q15_to_unit(int16_t q15) { return (float)q15 / 32768.0f; }
static float clampf(float v, float lo, float hi){ return (v<lo)?lo:((v>hi)?hi:v); }

// Convierte el comando SPI a velocidad del EJE DEL MOTOR [rad/s]
static float compute_motor_vel_from_cmd(const DriverSPI::ActuationCmdV1& cmd) {
  float unit = q15_to_unit(cmd.tgt_q15); // -1..+1
  float vel_motor = 0.0f;

  if (COMMAND_IS_OUTPUT_SIDE) {
    // El master piensa en la salida: su máximo práctico es MAX_VEL/GEAR
    float vel_out = unit * (MAX_VEL_RAD_S_MOTOR / GEAR_RATIO);
    vel_motor = vel_out * GEAR_RATIO; // paso a motor
  } else {
    // El master ya manda velocidad de motor normalizada
    vel_motor = unit * MAX_VEL_RAD_S_MOTOR;
  }

  if (fabsf(vel_motor) < DEADBAND_RAD_S) vel_motor = 0.0f;
  return clampf(vel_motor, -MAX_VEL_RAD_S_MOTOR, +MAX_VEL_RAD_S_MOTOR);
}

// Rampa (slew-rate) en rad/s^2 para evitar tirones
static float apply_slew(float target, float current, float dt_s) {
  float max_step = RAMP_RAD_S2 * dt_s;
  float delta = target - current;
  if (delta >  max_step) delta =  max_step;
  if (delta < -max_step) delta = -max_step;
  return current + delta;
}
void initSimpleFOC() {
  // Inicializar sensor
  pinMode(EN1, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(EN3, OUTPUT);
  digitalWrite(EN1, HIGH);
  digitalWrite(EN2, HIGH);
  digitalWrite(EN3, HIGH);
  // Configurar driver
  driver.voltage_power_supply = 5;   // Voltaje de alimentación del driver
  driver.init();
  driver.pwm_frequency = 30000; // Frecuencia PWM en Hz
  motor.linkDriver(&driver);


  motor.controller = MotionControlType::velocity_openloop;
  motor.voltage_limit  = 3.0f;      // subí según tu hardware
  motor.velocity_limit = MAX_VEL_RAD_S_MOTOR;    // tope mecánico del motor [rad/s] (seguro)

  // Inicializar motor y FOC
  motor.init();
  motor.initFOC();

  Serial.println("Motor con FOC inicializado!");
}
static inline void hard_stop_driver() {
  motor.move(0.0f);                 // PWM a cero
  digitalWrite(EN1, LOW);
  digitalWrite(EN2, LOW);
  digitalWrite(EN3, LOW);
}
static inline void enable_driver_if_safe() {
  digitalWrite(EN1, HIGH);
  digitalWrite(EN2, HIGH);
  digitalWrite(EN3, HIGH);
}
// --- Función para mover el motor en velocidad ---
void MotorVelMove(float vel) {
  motor.loopFOC();
  motor.move(vel); 
}


// --- Función para leer y mostrar datos ---
std::vector<float> readMPU6050() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  Serial.print("Aceleración [m/s^2] X:");
  Serial.print(a.acceleration.x);
  Serial.print(" Y:");
  Serial.print(a.acceleration.y);
  Serial.print(" Z:");
  Serial.println(a.acceleration.z);

  Serial.print("Giroscopio [rad/s] X:");
  Serial.print(g.gyro.x);
  Serial.print(" Y:");
  Serial.print(g.gyro.y);
  Serial.print(" Z:");
  Serial.println(g.gyro.z);
  float gravity = 9.80665f;
  return {a.acceleration.x/gravity, 
    a.acceleration.y/gravity, 
    a.acceleration.z/gravity , 
    g.gyro.x, g.gyro.y, g.gyro.z};
}

float encoder_5600(){
  uint16_t raw = encoder_i2c.readAngle();       // 0–4095
  float grados = raw * 360.0 / 4096;  // Conversión a grados
  Serial.print("Ángulo: ");
  Serial.print(grados, 2);
  Serial.println("°");
  return ((raw * 2.0f * (float)M_PI) / 4096.0f);
}





///////////////////////
void setup() {

  Serial.begin(115200);

  Wire1.setSDA(14);
  Wire1.setSCL(15);
  Wire1.begin();  
  // mpu.begin();     
  encoder_i2c.begin();

  // --- SPI: remapear MISO a GPIO12 para no chocar con LED_PIN=16 ---
  DriverSPI::setPins({ .sck=10, .mosi=11, .miso=12, .cs=13});
  DriverSPI::begin();

  // // Auto-zero offsets de ACS712 (sin corriente)
  DriverSPI::CAL_A.offset_V = DriverSPI::autocal_offset_V(DriverSPI::CAL_A.adc_ch, 256);
  DriverSPI::CAL_B.offset_V = DriverSPI::autocal_offset_V(DriverSPI::CAL_B.adc_ch, 256);
  DriverSPI::CAL_C.offset_V = DriverSPI::autocal_offset_V(DriverSPI::CAL_C.adc_ch, 256);
  
  // initSimpleFOC();
  pixels.begin();
  pixels.clear();
  pixels.show();
  pinMode(7, OUTPUT);
  digitalWrite(7, LOW);
}

void loop() {
  // dt para rampa
  Serial.println("B");
  uint32_t now_us = to_us_since_boot(get_absolute_time());
  if (g_last_tick_us == 0) g_last_tick_us = now_us;
  float dt = (now_us - g_last_tick_us) * 1e-6f;
  g_last_tick_us = now_us;
  // Despachar transacción SPI desde main loop (no en ISR)
  if (DriverSPI::g_xfer_pending) {
    // Solo si CS sigue LOW intentamos transferir
    if (!gpio_get(DriverSPI::PIN.cs)) {
      DriverSPI::serve_transaction_safe(200);  // 200 us/byte; ajustable
    }
    DriverSPI::g_xfer_pending = false;         // limpiar flag SIEMPRE
  }
//  --- Sampleo cada ~2 ms (encoder, IMU, corrientes 3φ) ---
  static uint32_t last_sense_us = 0;
  if (now_us - last_sense_us >= 2000) {
    last_sense_us = now_us;

    DriverSPI::SensorSnap s{};
    s.enc_rad = DriverSPI::read_encoder_rad_noprint();

    // IMU
    // DriverSPI::sample_mpu(s.ax_g, s.ay_g, s.az_g, s.gx_rads, s.gy_rads, s.gz_rads);

    // Corriente 3 fases (ACS712-05B)
    DriverSPI::read_currents_3ph(s.iA, s.iB, s.iC);
    float aA = fabsf(s.iA), aB = fabsf(s.iB), aC = fabsf(s.iC);
    s.i_peak = aA; s.phase_peak = 0;
    if (aB > s.i_peak) { s.i_peak = aB; s.phase_peak = 1; }
    if (aC > s.i_peak) { s.i_peak = aC; s.phase_peak = 2; }

    // Latch de sobrecorriente hardware (AJUSTAR UMBRAL)
    static constexpr float    I_HW_TRIP_A     = 6.5f;  // p. inicial para 05B
    static constexpr uint32_t HW_TRIP_HOLD_MS = 150;
    uint32_t now_ms = now_us / 1000u;
    if (s.i_peak > I_HW_TRIP_A) {
      DriverSPI::g_oc_hold_until_ms = now_ms + HW_TRIP_HOLD_MS;
    }
    s.overcurrent = (now_ms < DriverSPI::g_oc_hold_until_ms);
    s.t_us = now_us;

    // Publicar snapshot atómico
    uint32_t irq = save_and_disable_interrupts();
    memcpy((void*)&DriverSPI::g_snap, &s, sizeof(DriverSPI::SensorSnap)); 
    restore_interrupts(irq);
  }

  // snapshot de OC
  bool oc_active = ((to_us_since_boot(get_absolute_time())/1000u) < DriverSPI::g_oc_hold_until_ms);

  // snapshot del último comando recibido por SPI
  DriverSPI::ActuationCmdV1 cmd{};
  uint8_t  seq   = 0;
  uint32_t age_us = 0xFFFFFFFF;
  bool have_cmd = DriverSPI::copyLastCmd(cmd, seq, age_us);
// condiciones de seguridad
  bool cmd_fresh   = have_cmd && (age_us/1000u) <= FAILSAFE_MS;   // 50 ms
  bool estop_latch = DriverSPI::g_estop_latched;

  // failsafe & target
  float target_motor = 0.0f;
// Árbol de decisión
  if (estop_latch) {
    // 1) E-STOP por protocolo → bloqueado hasta reset
    hard_stop_driver();

  } else if (oc_active) {
    // 2) Sobre-corriente detectada local → hard stop
    hard_stop_driver();

  } else if (!cmd_fresh) {
    // 3) No llegan comandos (ESP32 paró SPI) → hard stop
    hard_stop_driver();

  } else {
    // 4) Normal: aplica rampa y mueve
    target_motor = compute_motor_vel_from_cmd(cmd);   // [rad/s] eje motor
    g_target_vel_motor = apply_slew(target_motor, g_target_vel_motor, dt);

    enable_driver_if_safe();
    motor.loopFOC();
    motor.move(g_target_vel_motor);
  }

  // LED feedback proporcional a |vel|
  pixels.setPixelColor(0, pixels.Color(0, 255, 255)); // Verde máximo
  pixels.show();
  delay(1);
}