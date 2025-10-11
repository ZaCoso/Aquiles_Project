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

namespace DriverSPI {

    // ===================== Config y protocolo =====================
    struct Pins {
      uint8_t sck  = 18;   // SPI0 SCK
      uint8_t mosi = 19;   // SPI0 TX (MOSI del master)
      uint8_t miso = 16;   // SPI0 RX (MISO del master)
      uint8_t cs   = 17;   // Chip Select desde el ESP32
      uint8_t drdy = 22;   // opcional: DATA_READY → ESP32 (0 = deshabilitado)
    };

    static Pins PIN;  // editable antes de begin()

    static constexpr uint8_t  PROTO_VERSION  = 0x01;
    static constexpr uint8_t  ACT_SOF        = 0x5A;
    static constexpr uint8_t  TEL_SOF        = 0xA5;
    static constexpr size_t   ACT_SIZE       = 16;
    static constexpr size_t   TEL_SIZE       = 36;
    static constexpr uint8_t  TEL_LEN_FIELD  = 35;

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
    static volatile uint8_t  g_seq       = 0;
    static volatile bool     g_started   = false;
    static volatile ActuationCmdV1 g_last_cmd{};

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

    static void fillTelemetry(TelemetryFrameV1& f) {

      f.sof       = TEL_SOF;
      f.version   = PROTO_VERSION;
      f.len       = TEL_LEN_FIELD;
      f.seq       = g_seq;
      f.t_us      = micros32();
      f.flags     = 0;
      f.sample_hz = 1000;                 // 1 kHz
      f.enc_q15   = rad_to_q15(encoder_5600());
      auto mpu_data = readMPU6050();  // actualizar datos del MPU6050
      // IMU demo (dejar en 0/1g)
      f.mpu_ax_q14 = g_to_q14(mpu_data[0]);
      f.mpu_ay_q14 = g_to_q14(mpu_data[1]);
      f.mpu_az_q14 = g_to_q14(mpu_data[2]);
      f.mpu_gx_q14 = gyro_to_q15(mpu_data[3], 4.3633f);
      f.mpu_gy_q14 = gyro_to_q15(mpu_data[4], 4.3633f);
      f.mpu_gz_q14 = gyro_to_q15(mpu_data[5], 4.3633f);

      f.err_code  = 0;
      f.frame_ctr = (uint16_t)(g_frame_ctr & 0xFFFF);

      for (int i=0;i<5;i++) f.reserved[i] = 0;
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

      if (PIN.drdy) { gpio_put(PIN.drdy, 1); sleep_us(5); gpio_put(PIN.drdy, 0); }
      return true;
    }

    // ===================== Transacción SPI: RX 16B + TX 36B =====================
    static void serve_transaction() {
      // 1) Recibir comando (16B)
      ActuationCmdV1 cmd{}; 
      for (int i=0;i<(int)ACT_SIZE;i++) {
        uint8_t b; spi_read_blocking(spi0, 0x00, &b, 1);
        reinterpret_cast<uint8_t*>(&cmd)[i] = b;
      }
      bool ok = (cmd.sof==ACT_SOF && cmd.version==PROTO_VERSION && cmd.len==(ACT_SIZE-1));
      uint8_t ccrc = crc8_0x07(reinterpret_cast<const uint8_t*>(&cmd), ACT_SIZE-1, 0x00);
      ok = ok && (ccrc == cmd.crc8);
      if (ok) {
        // g_last_cmd es volatile → usar memcpy para evitar el warning
        memcpy((void*)&g_last_cmd, &cmd, sizeof(ActuationCmdV1));
      }

      // 2) Snapshot del último frame “cerrado” (volatile → local)
      TelemetryFrameV1 snap{};
      {
        uint32_t irq = save_and_disable_interrupts();
        memcpy(&snap, (const void*)&g_tx_buf[g_idx_rd], sizeof(TelemetryFrameV1));
        restore_interrupts(irq);
      }

      // 3) Transmitir (36B)
      for (int i=0;i<(int)TEL_SIZE;i++) {
        uint8_t b = reinterpret_cast<uint8_t*>(&snap)[i];
        spi_write_blocking(spi0, &b, 1);
      }
    }

    // ===================== IRQ por CS (flanco ↓ = 1 transacción) =================
    // Firma correcta para Philhower/Pico SDK:
    static void cs_isr(uint gpio, uint32_t events) {
      (void)events;
      if (gpio == PIN.cs && !gpio_get(PIN.cs)) {
        serve_transaction();
      }
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
      if (PIN.drdy) { gpio_init(PIN.drdy); gpio_set_dir(PIN.drdy, GPIO_OUT); gpio_put(PIN.drdy, 0); }

      // SPI0 esclavo, modo 0
      spi_init(spi0, 1000*1000);       // freq dummy
      spi_set_slave(spi0, true);
      spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

      // Registrar callback de GPIO con la firma correcta
      gpio_set_irq_enabled_with_callback(PIN.cs, GPIO_IRQ_EDGE_FALL, true, cs_isr);

      // Buffers iniciales
      TelemetryFrameV1 z{}; fillTelemetry(z);
      memcpy((void*)&g_tx_buf[0], &z, sizeof(TelemetryFrameV1));
      memcpy((void*)&g_tx_buf[1], &z, sizeof(TelemetryFrameV1));

      // Timer 1 kHz
      add_repeating_timer_us(-1000, timer_cb, nullptr, &g_tim);

      Serial.println("[DriverSPI] listo (SPI0 slave, 1 kHz).");
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
void initSimpleFOC() {
  // Inicializar sensor
  
  // Configurar driver
  driver.voltage_power_supply = 5;   // Voltaje de alimentación del driver
  driver.init();
  driver.pwm_frequency = 30000; // Frecuencia PWM en Hz
  motor.linkDriver(&driver);


  motor.controller = MotionControlType::velocity_openloop;
  
  // Inicializar motor y FOC
  motor.init();
  motor.initFOC();

  Serial.println("Motor con FOC inicializado!");
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

void encoder_init(){
  if (!encoder_i2c.begin()) {
    Serial.println("AS5600 no encontrado!");
    while (1) delay(200);
  }
  Serial.println("AS5600 listo.");
  // SPI.begin();   
  // encoder_spi.begin(); // CS en pin 5
}



///////////////////////
void setup() {
  Serial.begin(115200);
  Wire1.setSDA(14);
  Wire1.setSCL(15);
  Wire1.begin();  
  mpu.begin();     
  DriverSPI::begin();


  // encoder_i2c.begin();
  // encoder_i2c.setDirection(AS5600_CLOCK_WISE);
  // init_spi_slave();
  initSimpleFOC();
  pixels.begin();
  pixels.clear();
  pixels.show();
}

void loop() {
  motor.loopFOC();
  MotorVelMove(1);
  // encoder_5600();

  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
  delay(100);
  pixels.clear();
  pixels.show();
  delay(100);
}