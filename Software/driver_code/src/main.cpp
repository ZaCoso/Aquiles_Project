#include <Arduino.h>
#include <Wire.h>
#define Wire Wire1  
#include <SimpleFOC.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/timer.h"

#define SPI_SELFTEST 1   // <--- Activa modo autoprueba sin sensores

// ==== Pines ====
#define EN1 0
#define EN2 4
#define EN3 2  
#define LED_PIN   16

// --- FOC dummy (no se usa para self-test) ---
BLDCMotor motor = BLDCMotor(7);
BLDCDriver3PWM driver = BLDCDriver3PWM(1, 5, 3);

namespace DriverSPI {

    static volatile uint32_t g_last_cmd_us = 0;

    struct Pins {
      uint8_t sck  = 18;
      uint8_t mosi = 19;
      uint8_t miso = 16;
      uint8_t cs   = 17;
      uint8_t drdy = 22;
    };
    static Pins PIN;

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

    static repeating_timer_t g_tim;
    static volatile TelemetryFrameV1 g_tx_buf[2];
    static volatile uint8_t  g_idx_wr = 0;
    static volatile uint8_t  g_idx_rd = 1;
    static volatile uint32_t g_frame_ctr = 0;
    static volatile uint8_t  g_seq       = 0;
    static volatile bool     g_started   = false;
    static volatile ActuationCmdV1 g_last_cmd{};

    // ==== Utils ====
    static inline uint8_t crc8_0x07(const uint8_t* d, size_t n, uint8_t init = 0x00) {
      uint8_t c = init;
      for (size_t i=0;i<n;i++){ c ^= d[i]; for (uint8_t k=0;k<8;k++) c = (c & 0x80) ? (uint8_t)((c<<1) ^ 0x07) : (uint8_t)(c<<1); }
      return c;
    }
    static inline uint32_t micros32() { return (uint32_t) to_us_since_boot(get_absolute_time()); }
    static inline int16_t rad_to_q15(float rad) { return  (int16_t)((rad / (2.0f * (float)M_PI)) * 32768.0f); }

    // ==== Telemetría sintética para autoprueba ====
    static void fillTelemetry(TelemetryFrameV1& f) {
    #if SPI_SELFTEST
      ActuationCmdV1 last{};
      {
        uint32_t irq = save_and_disable_interrupts();
        memcpy(&last, (const void*)&g_last_cmd, sizeof(last));
        restore_interrupts(irq);
      }

      f.sof       = TEL_SOF;
      f.version   = PROTO_VERSION;
      f.len       = TEL_LEN_FIELD;
      f.seq       = g_seq;
      f.t_us      = micros32();
      f.flags     = 0x5AA5;     // patrón conocido
      f.sample_hz = 1000;
      int16_t ramp = (int16_t)((g_frame_ctr * 1024u) & 0x7FFF);
      f.enc_q15   = ramp;
      f.mpu_ax_q14 = f.mpu_ay_q14 = f.mpu_az_q14 = 0;
      f.mpu_gx_q14 = f.mpu_gy_q14 = f.mpu_gz_q14 = 0;
      f.err_code  = 0;
      f.frame_ctr = (uint16_t)(g_frame_ctr & 0xFFFF);

      // ECO del último comando recibido
      f.reserved[0] = last.seq;
      f.reserved[1] = last.mode;
      f.reserved[2] = (uint8_t)(last.tgt_q15 & 0xFF);
      f.reserved[3] = (uint8_t)((last.tgt_q15 >> 8) & 0xFF);
      f.reserved[4] = last.crc8;

      f.crc8 = crc8_0x07(reinterpret_cast<const uint8_t*>(&f), TEL_SIZE-1, 0x00);
    #else
      // modo normal con sensores
    #endif
    }

    static bool timer_cb(repeating_timer_t*) {
      TelemetryFrameV1 w{};
      fillTelemetry(w);
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

    static void serve_transaction() {
      ActuationCmdV1 cmd{}; 
      for (int i=0;i<(int)ACT_SIZE;i++) {
        uint8_t b; spi_read_blocking(spi0, 0x00, &b, 1);
        reinterpret_cast<uint8_t*>(&cmd)[i] = b;
      }

      bool ok = (cmd.sof==ACT_SOF && cmd.version==PROTO_VERSION && cmd.len==(ACT_SIZE-1));
      uint8_t ccrc = crc8_0x07(reinterpret_cast<const uint8_t*>(&cmd), ACT_SIZE-1, 0x00);
      ok = ok && (ccrc == cmd.crc8);
      if (ok) {
        memcpy((void*)&g_last_cmd, &cmd, sizeof(ActuationCmdV1));
        g_last_cmd_us = micros32();
      }

      TelemetryFrameV1 snap{};
      uint32_t irq = save_and_disable_interrupts();
      memcpy(&snap, (const void*)&g_tx_buf[g_idx_rd], sizeof(TelemetryFrameV1));
      restore_interrupts(irq);

      for (int i=0;i<(int)TEL_SIZE;i++) {
        uint8_t b = reinterpret_cast<uint8_t*>(&snap)[i];
        spi_write_blocking(spi0, &b, 1);
      }
    }

    static void cs_isr(uint gpio, uint32_t events) {
      (void)events;
      if (gpio == PIN.cs && !gpio_get(PIN.cs)) serve_transaction();
    }

    void begin() {
      gpio_set_function(PIN.sck,  GPIO_FUNC_SPI);
      gpio_set_function(PIN.mosi, GPIO_FUNC_SPI);
      gpio_set_function(PIN.miso, GPIO_FUNC_SPI);
      gpio_init(PIN.cs); gpio_set_dir(PIN.cs, GPIO_IN); gpio_pull_up(PIN.cs);
      if (PIN.drdy) { gpio_init(PIN.drdy); gpio_set_dir(PIN.drdy, GPIO_OUT); gpio_put(PIN.drdy, 0); }

      spi_init(spi0, 1000000);
      spi_set_slave(spi0, true);
      spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

      gpio_set_irq_enabled_with_callback(PIN.cs, GPIO_IRQ_EDGE_FALL, true, cs_isr);

      TelemetryFrameV1 z{}; fillTelemetry(z);
      memcpy((void*)&g_tx_buf[0], &z, sizeof(TelemetryFrameV1));
      memcpy((void*)&g_tx_buf[1], &z, sizeof(TelemetryFrameV1));

      add_repeating_timer_us(-1000, timer_cb, nullptr, &g_tim);
      Serial.println("[DriverSPI] listo (self-test SPI, 1kHz).");
    }

} // namespace DriverSPI


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("RP2040 SELFTEST - SPI SLAVE");
  DriverSPI::begin();
}

void loop() {
  delay(1000);
  // nada más: el RP2040 responde al master automáticamente
}