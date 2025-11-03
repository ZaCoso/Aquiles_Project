#include <Arduino.h>
#include <Wire.h>
#define Wire Wire1        // use secondary I2C bus on RP2040
#include <AS5600.h>

#include <Adafruit_NeoPixel.h>
#include <SimpleFOC.h>

// RP2040 SDK headers
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/adc.h"

#include <vector>
#include <cstring>
#include <cmath>

/* =================================================================
 *  HARDWARE CONSTANTS AND GLOBAL PERIPHERALS
 *  This section defines board pins and global device objects
 *  so the rest of the code can just "use" them.
 * ================================================================= */
namespace HW {
  // External driver enable pins – your power stage likely needs these HIGH to enable
  constexpr int EN1 = 0;
  constexpr int EN2 = 4;
  constexpr int EN3 = 2;

  // Onboard WS2812 / NeoPixel (Waveshare RP2040 Zero)
  constexpr int LED_PIXEL_PIN   = 16;
  constexpr int LED_PIXEL_COUNT = 1;

  // "Breathing" LED on a regular GPIO via PWM
  constexpr int LED_BREATHE_PIN   = 7;
  constexpr float LED_BREATHE_PER = 2.5f;  // seconds per full fade in/out
  constexpr int LED_BREATHE_RANGE = 255;   // 8-bit PWM range

  // I2C pins for the AS5600 magnetic encoder
  constexpr int I2C_SDA = 14;
  constexpr int I2C_SCL = 15;
}

// Global devices (they must be global because driver code uses them)
Adafruit_NeoPixel g_pixels(HW::LED_PIXEL_COUNT,
                           HW::LED_PIXEL_PIN,
                           NEO_GRB + NEO_KHZ800);

AS5600 g_encoder_i2c;

/* =================================================================
 *  MOTOR CONTROL (SimpleFOC)
 *  - owns the BLDCMotor and BLDCDriver objects
 *  - provides helper functions for safe enabling and for slew-rate
 *  - translates normalized commands to motor rad/s
 * ================================================================= */
namespace MotorCtrl {

  // Actual motor object: 7 pole pairs
  BLDCMotor motor(7);

  // 3-PWM driver: pins are specific to your RP2040 wiring
  BLDCDriver3PWM driver(1, 5, 3);

  // ---- tuning / safety parameters ----
  constexpr float GEAR_RATIO             = 51.0f;   // your external gearbox
  constexpr float MAX_VEL_RAD_S_MOTOR    = 100.0f;  // max safe motor speed
  constexpr float RAMP_RAD_S2            = 800.0f;  // how fast we allow the speed to change
  constexpr float DEADBAND_RAD_S         = 0.5f;    // ignore tiny speeds (avoid buzzing)
  constexpr uint32_t FAILSAFE_MS         = 50;      // if master stops sending → stop in 50 ms
  constexpr bool COMMAND_IS_OUTPUT_SIDE  = true;    // master thinks in output-shaft speed

  // current target used by the slew-rate limiter
  float g_target_vel_motor = 0.0f;

  // ---- small helpers ----
  inline float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
  }

  inline float q15_to_unit(int16_t q15) {
    // converts -32768..32767 to approx -1..+1
    return (float)q15 / 32768.0f;
  }

  // Convert an actuation command (Q15) to motor rad/s
  // T is the command struct type, we only need tgt_q15
  template<typename CmdT>
  float compute_motor_vel_from_cmd(const CmdT& cmd) {
    float unit = q15_to_unit(cmd.tgt_q15);  // normalized −1..+1
    float vel_motor = 0.0f;

    if (COMMAND_IS_OUTPUT_SIDE) {
      // Master sends "I want this speed at the gearbox OUTPUT"
      // We must convert to motor speed by multiplying by gear ratio
      float vel_out = unit * (MAX_VEL_RAD_S_MOTOR / GEAR_RATIO);
      vel_motor = vel_out * GEAR_RATIO;
    } else {
      // Master already sends motor speed in normalized form
      vel_motor = unit * MAX_VEL_RAD_S_MOTOR;
    }

    // remove tiny values so the motor doesn't vibrate
    if (fabsf(vel_motor) < DEADBAND_RAD_S) vel_motor = 0.0f;

    // final clamp for safety
    return clampf(vel_motor, -MAX_VEL_RAD_S_MOTOR, +MAX_VEL_RAD_S_MOTOR);
  }

  // Slew-rate limiter: moves "current" toward "target" at a bounded acceleration
  inline float apply_slew(float target, float current, float dt_s) {
    float max_step = RAMP_RAD_S2 * dt_s;
    float delta    = target - current;
    if (delta >  max_step) delta =  max_step;
    if (delta < -max_step) delta = -max_step;
    return current + delta;
  }

  // Disable power stage immediately
  inline void hard_stop_driver() {
    motor.move(0.0f);
    digitalWrite(HW::EN1, LOW);
    digitalWrite(HW::EN2, LOW);
    digitalWrite(HW::EN3, LOW);
  }

  // Enable power stage (only call this when you know it’s safe)
  inline void enable_driver_if_safe() {
    digitalWrite(HW::EN1, HIGH);
    digitalWrite(HW::EN2, HIGH);
    digitalWrite(HW::EN3, HIGH);
  }

  // Initialize SimpleFOC motor + driver
  void init() {
    // enable-pins for your external bridge
    pinMode(HW::EN1, OUTPUT);
    pinMode(HW::EN2, OUTPUT);
    pinMode(HW::EN3, OUTPUT);
    enable_driver_if_safe();

    // configure power driver
    driver.voltage_power_supply = 5.0f;   // depends on your hardware
    driver.pwm_frequency        = 30000;  // 30 kHz -> quieter motor
    driver.init();

    // link driver to motor
    motor.linkDriver(&driver);

    // we run open-loop velocity here (no position sensor in FOC loop)
    motor.controller     = MotionControlType::velocity_openloop;
    motor.voltage_limit  = 3.0f;                // protect the bridge
    motor.velocity_limit = MAX_VEL_RAD_S_MOTOR; // guardrail

    // final init
    motor.init();
    motor.initFOC();

    Serial.println("[MotorCtrl] FOC motor initialized");
  }

  // Run FOC inner loop and apply velocity
  inline void move_velocity(float vel) {
    motor.loopFOC();  // even in open-loop, keep this
    motor.move(vel);
  }

} // namespace MotorCtrl

/* =================================================================
 *  BREATHING LED VIA PWM
 *  - just cosmetic, to see main loop is alive
 * ================================================================= */
namespace LedBreath {
  float phase = 0.0f;

  void begin() {
    pinMode(HW::LED_BREATHE_PIN, OUTPUT);
    analogWriteFreq(1000);               // ~1 kHz PWM
    analogWriteRange(HW::LED_BREATHE_RANGE);
    analogWrite(HW::LED_BREATHE_PIN, HW::LED_BREATHE_RANGE);
  }

  // dt_s = elapsed time since last call
  void update(float dt_s) {
    // move phase 0..1
    phase += dt_s / HW::LED_BREATHE_PER;
    if (phase >= 1.0f) phase -= 1.0f;

    // create a smooth 0..1 waveform using cosine
    float x = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * phase));
    // simple gamma to look more natural
    float y = x * x;

    int duty = (int)(y * HW::LED_BREATHE_RANGE + 0.5f);
    analogWrite(HW::LED_BREATHE_PIN, duty);
  }
}

/* =================================================================
 *  UART PROTOCOL + TELEMETRY (with SLIP)
 *  - sends telemetry frames periodically
 *  - receives actuation frames (you must finish the SLIP parser)
 *  - holds sensor snapshot so ISR/timer and loop don't fight
 * ================================================================= */
namespace DriverUART {

  /* ---------- UART physical config ---------- */
  constexpr uint32_t UART_BAUD   = 2000000;  // 2 Mbps
  constexpr uint     UART_TX_PIN = 12;
  constexpr uint     UART_RX_PIN = 13;
  #define UART_ID uart0

  /* ---------- protocol constants ---------- */
  constexpr uint8_t  ACT_SOF       = 0x5A;
  constexpr uint8_t  TEL_SOF       = 0xA5;
  constexpr size_t   ACT_SIZE      = 16;
  constexpr size_t   TEL_SIZE      = 36;   // original code used 36
  constexpr uint8_t  TEL_LEN_FIELD = 35;
  constexpr uint16_t FLAG_OC       = (1u << 0);
  constexpr uint8_t  PROTO_VERSION_V2 = 0x02;

  /* ---------- SLIP special bytes ---------- */
  constexpr uint8_t SLIP_END     = 0xC0;
  constexpr uint8_t SLIP_ESC     = 0xDB;
  constexpr uint8_t SLIP_ESC_END = 0xDC;
  constexpr uint8_t SLIP_ESC_ESC = 0xDD;

  /* ---------- command + telemetry structs ---------- */
  #pragma pack(push,1)
  // This is what the master sends to this board
  struct ActuationCmdV1 {
    uint8_t  sof, version, len, seq;  // header
    uint8_t  mode, rsv0;              // not used right now
    int16_t  tgt_q15, kp_q15, kd_q15; // target = velocity normalized
    uint16_t limits;
    uint8_t  rsv1, crc8;
  };

  // This is what we send back to the master periodically
  struct TelemetryFrameV2 {
    uint8_t  sof;        // 0xA5
    uint8_t  version;    // 0x02
    uint8_t  len;        // payload length = sizeof(...) - 1
    uint8_t  seq;        // increments each frame
    uint32_t t_us;       // microsecond timestamp
    uint16_t flags;      // bit0 = overcurrent, etc.
    int16_t  enc_q15;    // encoder angle scaled to Q15
    int16_t  ipeak_q15;  // not used -> can be 0
    uint8_t  phase_peak; // which phase had max current
    uint16_t frame_ctr;  // rolling frame counter
    uint8_t  crc8;       // checksum
  };
  #pragma pack(pop)

  constexpr size_t  TEL2_SIZE = sizeof(TelemetryFrameV2);
  constexpr uint8_t TEL2_LEN  = TEL2_SIZE - 1;

  /* ---------- internal state ---------- */
  static volatile bool g_inited = false;

  // timer that periodically writes telemetry into a double buffer
  static repeating_timer_t g_tim;
  static volatile TelemetryFrameV2 g_tx_buf[2];
  static volatile uint8_t  g_idx_wr = 0;   // buffer where timer writes
  static volatile uint8_t  g_idx_rd = 1;   // buffer the main loop sends
  static volatile uint32_t g_frame_ctr = 0;
  static volatile uint8_t  g_seq = 0;

  // last command received from master
  static volatile uint32_t      g_last_cmd_us = 0;
  static volatile ActuationCmdV1 g_last_cmd{};

  // overcurrent latch shared with loop
  static volatile uint32_t g_oc_hold_until_ms = 0;

  // this is the sensor data produced by the main loop
  struct SensorSnap {
    float    enc_rad;
    float    iA, iB, iC;
    float    i_peak;
    uint8_t  phase_peak;
    bool     overcurrent;
    uint32_t t_us;
  };
  static volatile SensorSnap g_snap = {};

  /* ---------- small helpers ---------- */
  inline uint32_t micros32() {
    return (uint32_t)to_us_since_boot(get_absolute_time());
  }

  inline uint8_t crc8_0x07(const uint8_t* d, size_t n, uint8_t init = 0x00) {
    uint8_t c = init;
    for (size_t i = 0; i < n; i++) {
      c ^= d[i];
      for (uint8_t k = 0; k < 8; k++)
        c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
  }

  inline int16_t rad_to_q15(float rad) {
    // map 0..2π to 0..32768
    return (int16_t)((rad / (2.0f * M_PI)) * 32768.0f);
  }

  // raw UART helpers
  inline void uart_write_byte(uint8_t b) { uart_putc_raw(UART_ID, (char)b); }
  inline bool uart_read_available()     { return uart_is_readable(UART_ID); }
  inline uint8_t uart_read_byte()       { return (uint8_t)uart_getc(UART_ID); }

  /* ---------- SLIP send helpers ---------- */
  void slip_write_byte(uint8_t b) {
    if (b == SLIP_END) {
      uart_write_byte(SLIP_ESC);
      uart_write_byte(SLIP_ESC_END);
    } else if (b == SLIP_ESC) {
      uart_write_byte(SLIP_ESC);
      uart_write_byte(SLIP_ESC_ESC);
    } else {
      uart_write_byte(b);
    }
  }

  // wraps a frame between SLIP_END bytes and escapes special bytes
  void slip_write_frame(const uint8_t* data, size_t n) {
    uart_write_byte(SLIP_END);
    for (size_t i = 0; i < n; i++) slip_write_byte(data[i]);
    uart_write_byte(SLIP_END);
  }

  /* ---------- SLIP receive (incomplete) ----------
   * Here you must put your original SLIP state machine:
   *   - accumulate bytes between END markers
   *   - unescape ESC/DC and ESC/DD
   *   - when you have a full frame: validate CRC, then write g_last_cmd
   *   - also record timestamp: g_last_cmd_us = micros32();
   */
  void slip_feed(uint8_t /*b*/) {
    // TODO: paste your original SLIP parser here
  }

  /* ---------- ADC → currents ----------
   * We use 3 ADC channels (26, 27, 28 on RP2040) to read
   * the 3 phase sensors (like ACS712). We convert to Amps
   * using a simple linear model.
   */
  struct PhaseCal { float gain_VperA; float offset_V; int adc_ch; };
  constexpr float VREF_V     = 3.3f;
  constexpr float SENS_VperA = 0.185f;   // adjust to your sensor
  static PhaseCal CAL_A{SENS_VperA, VREF_V * 0.5f, 0};
  static PhaseCal CAL_B{SENS_VperA, VREF_V * 0.5f, 1};
  static PhaseCal CAL_C{SENS_VperA, VREF_V * 0.5f, 2};

  inline float adc_to_V(int adc_ch) {
    adc_select_input(adc_ch);
    uint16_t raw = adc_read();
    return (raw * VREF_V) / 4095.0f;
  }

  inline float volts_to_A(float v, const PhaseCal& c) {
    return (v - c.offset_V) / c.gain_VperA;
  }

  inline void read_currents_3ph(float& Ia, float& Ib, float& Ic) {
    float vA = adc_to_V(CAL_A.adc_ch);
    float vB = adc_to_V(CAL_B.adc_ch);
    float vC = adc_to_V(CAL_C.adc_ch);
    Ia = volts_to_A(vA, CAL_A);
    Ib = volts_to_A(vB, CAL_B);
    Ic = volts_to_A(vC, CAL_C);
  }

  // encoder helper: read and return angle in radians
  inline float read_encoder() {
    uint16_t raw = g_encoder_i2c.readAngle(); // 0..4095
    return (raw * (2.0f * (float)M_PI)) / 4096.0f;
  }

  /* ---------- Telemetry builder ----------
   * Fills a TelemetryFrameV2 struct from the latest sensor snapshot.
   * Note we COPY the snapshot with interrupts disabled to avoid race conditions.
   */
  void fillTelemetryV2(TelemetryFrameV2& f) {
    SensorSnap s{};
    uint32_t irq = save_and_disable_interrupts();
    memcpy(&s, (const void*)&g_snap, sizeof(SensorSnap));
    restore_interrupts(irq);

    f.sof        = TEL_SOF;
    f.version    = PROTO_VERSION_V2;
    f.len        = TEL2_LEN;
    f.seq        = g_seq;
    f.t_us       = micros32();
    f.flags      = 0; // could be: s.overcurrent ? FLAG_OC : 0;
    f.enc_q15    = rad_to_q15(s.enc_rad);
    f.ipeak_q15  = 0; // not used
    f.phase_peak = 0; // not used
    f.frame_ctr  = (uint16_t)(g_frame_ctr & 0xFFFF);
    f.crc8       = crc8_0x07(reinterpret_cast<const uint8_t*>(&f), TEL2_SIZE - 1);
  }

  // called every 1 ms by RP2040 timer – produces a new telemetry frame
  bool timer_cb(repeating_timer_t*) {
    TelemetryFrameV2 w{};
    fillTelemetryV2(w);

    // swap double buffer atomically
    uint32_t irq = save_and_disable_interrupts();
    memcpy((void*)&g_tx_buf[g_idx_wr], &w, sizeof(TelemetryFrameV2));
    uint8_t old_rd = g_idx_rd;
    g_idx_rd = g_idx_wr;
    g_idx_wr = old_rd;
    g_frame_ctr++;
    g_seq++;
    restore_interrupts(irq);

    return true;
  }

  // initialize UART, ADC and telemetry timer
  void begin() {
    if (g_inited) return;
    g_inited = true;

    // UART config
    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    // ADC init: use GPIO 26/27/28 → ADC 0/1/2
    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);

    // initialize telemetry buffers with a valid frame
    TelemetryFrameV2 z{}; 
    fillTelemetryV2(z);
    memcpy((void*)&g_tx_buf[0], &z, sizeof(TelemetryFrameV2));
    memcpy((void*)&g_tx_buf[1], &z, sizeof(TelemetryFrameV2));

    // periodic telemetry at 1 kHz
    add_repeating_timer_us(-1000, timer_cb, nullptr, &g_tim);

    Serial.println("[DriverUART] ready (UART0 at 2Mbps, pins GP12/GP13)");
  }

  // main-loop poll:
  // 1) read any incoming SLIP bytes
  // 2) send latest telemetry frame
  void poll_io() {
    // RX path
    while (uart_read_available()) {
      uint8_t b = uart_read_byte();
      slip_feed(b);
    }

    // TX path: send the latest telemetry from the double buffer
    TelemetryFrameV2 snap{};
    uint32_t irq = save_and_disable_interrupts();
    memcpy(&snap, (const void*)&g_tx_buf[g_idx_rd], sizeof(TelemetryFrameV2));
    restore_interrupts(irq);

    // NOTE: your original code was sending TEL_SIZE (36).
    // Here we send the actual size of TelemetryFrameV2 (20 bytes).
    // If your master expects 36 bytes, change TEL2_SIZE to 36.
    slip_write_frame(reinterpret_cast<const uint8_t*>(&snap), TEL2_SIZE);
  }

  // Copy last command in a safe way so the main loop can use it
  bool copyLastCmd(ActuationCmdV1& out, uint8_t& seq_out, uint32_t& age_us_out) {
    uint32_t irq = save_and_disable_interrupts();
    ActuationCmdV1 tmp;
    memcpy(&tmp, (const void*)&g_last_cmd, sizeof(ActuationCmdV1));
    uint8_t  sq  = g_seq;
    uint32_t age = micros32() - g_last_cmd_us;
    restore_interrupts(irq);

    out          = tmp;
    seq_out      = sq;
    age_us_out   = age;

    // validate basic header so we don’t react to garbage
    bool ok = (out.sof     == ACT_SOF &&
               out.version == PROTO_VERSION_V2 &&
               out.len     == (ACT_SIZE - 1));
    return ok;
  }

} // namespace DriverUART

/* =================================================================
 *  DEBUG ENCODER READ (kept from your original code)
 *  This function prints the angle to serial in degrees.
 * ================================================================= */
float encoder_5600() {
  uint16_t raw = g_encoder_i2c.readAngle(); // 0–4095
  float degrees = raw * 360.0f / 4096.0f;
  Serial.print("Angle: ");
  Serial.print(degrees, 2);
  Serial.println(" deg");
  return ((raw * 2.0f * (float)M_PI) / 4096.0f);
}

/* =================================================================
 *  SETUP
 * ================================================================= */
void setup() {
  Serial.begin(115200);

  // I2C for AS5600
  Wire1.setSDA(HW::I2C_SDA);
  Wire1.setSCL(HW::I2C_SCL);
  Wire1.begin();

  g_encoder_i2c.begin();
  g_encoder_i2c.setDirection(AS5600_CLOCK_WISE);

  // UART / telemetry init
  DriverUART::begin();

  // Motor / SimpleFOC init
  MotorCtrl::init();

  // NeoPixel init
  g_pixels.begin();
  g_pixels.clear();
  g_pixels.show();

  // breathing LED init
  LedBreath::begin();

  Serial.println("[setup] system ready");
}

/* =================================================================
 *  MAIN LOOP
 *  This loop:
 *   1) services UART protocol (RX commands / TX telemetry)
 *   2) periodically samples sensors
 *   3) applies failsafe & overcurrent latch
 *   4) commands motor through SimpleFOC
 *   5) updates visual indicators
 * ================================================================= */
void loop() {
  // 1) UART I/O
  DriverUART::poll_io();

  // 2) compute delta time since last loop
  static uint32_t last_us = 0;
  uint32_t now_us = to_us_since_boot(get_absolute_time());
  if (last_us == 0) last_us = now_us;
  float dt = (now_us - last_us) * 1e-6f;
  last_us  = now_us;

  // breathing LED animation
  LedBreath::update(dt);

  // 3) sample sensors every ~2 ms
  static uint32_t last_sense_us = 0;
  if (now_us - last_sense_us >= 2000) {
    last_sense_us = now_us;

    DriverUART::SensorSnap s{};

    // read encoder angle (in radians)
    s.enc_rad = DriverUART::read_encoder();

    // read 3-phase currents from ADC
    DriverUART::read_currents_3ph(s.iA, s.iB, s.iC);

    // find peak current and which phase it was
    float aA = fabsf(s.iA), aB = fabsf(s.iB), aC = fabsf(s.iC);
    s.i_peak     = aA; 
    s.phase_peak = 0;
    if (aB > s.i_peak) { s.i_peak = aB; s.phase_peak = 1; }
    if (aC > s.i_peak) { s.i_peak = aC; s.phase_peak = 2; }

    // overcurrent latch:
    // if we exceed threshold, we keep OC active for HW_TRIP_HOLD_MS ms
    constexpr float    I_HW_TRIP_A     = 6.5f;
    constexpr uint32_t HW_TRIP_HOLD_MS = 150;
    uint32_t now_ms = now_us / 1000u;
    if (s.i_peak > I_HW_TRIP_A)
      DriverUART::g_oc_hold_until_ms = now_ms + HW_TRIP_HOLD_MS;
    s.overcurrent = (now_ms < DriverUART::g_oc_hold_until_ms);

    s.t_us = now_us;

    // publish snapshot atomically so the timer can read it for telemetry
    uint32_t irq = save_and_disable_interrupts();
    memcpy((void*)&DriverUART::g_snap, &s, sizeof(DriverUART::SensorSnap));
    restore_interrupts(irq);
  }

  // 4) check if we are currently in overcurrent latch
  bool oc_active = ((to_us_since_boot(get_absolute_time()) / 1000u) < DriverUART::g_oc_hold_until_ms);

  // 5) fetch last command from master and see if it is recent
  DriverUART::ActuationCmdV1 cmd{};
  uint8_t  seq   = 0;
  uint32_t age_us = 0xFFFFFFFF;
  bool have_cmd   = DriverUART::copyLastCmd(cmd, seq, age_us);
  bool cmd_fresh  = have_cmd && (age_us / 1000u) <= MotorCtrl::FAILSAFE_MS;

  // 6) compute target motor speed (or 0 if no fresh cmd / OC)
  float target_motor = 0.0f;
  if (!oc_active && cmd_fresh) {
    target_motor = MotorCtrl::compute_motor_vel_from_cmd(cmd);
  }

  // apply slew-rate
  MotorCtrl::g_target_vel_motor =
    MotorCtrl::apply_slew(target_motor, MotorCtrl::g_target_vel_motor, dt);

  // 7) safe actuation
  if (oc_active) {
    // if overcurrent: stop motor and drop enables
    MotorCtrl::motor.move(0.0f);
    digitalWrite(HW::EN1, LOW);
    digitalWrite(HW::EN2, LOW);
    digitalWrite(HW::EN3, LOW);
  } else {
    // normal mode: enable bridge and run velocity
    digitalWrite(HW::EN1, HIGH);
    digitalWrite(HW::EN2, HIGH);
    digitalWrite(HW::EN3, HIGH);
    MotorCtrl::move_velocity(MotorCtrl::g_target_vel_motor);
  }

  // 8) NeoPixel visual feedback (static color for now)
  g_pixels.setPixelColor(0, g_pixels.Color(178, 255, 255));
  g_pixels.show();

  // 9) main loop pacing – 1 ms is fine here
  delay(1);
}