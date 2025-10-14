// ========= RP2040: Device UART (recibe CMD, responde TEL) =========
#include <Arduino.h>
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#define UART_ID     uart0
#define UART_TX_PIN 12     // GP12 -> TX0 (a RX del ESP32)
#define UART_RX_PIN 13     // GP13 -> RX0 (desde TX del ESP32)
#define UART_BAUD   2000000

// ========= Protocolo =========
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

static const uint8_t  PROTO_VERSION  = 0x01;
static const uint8_t  ACT_SOF        = 0x5A;
static const uint8_t  TEL_SOF        = 0xA5;
static const size_t   ACT_SIZE       = 16;
static const size_t   TEL_SIZE       = 36;
static const uint8_t  TEL_LEN_FIELD  = 35;
static const uint16_t FLAG_OC        = (1u<<0); // demo

// SLIP
static const uint8_t SLIP_END=0xC0, SLIP_ESC=0xDB, SLIP_ESC_END=0xDC, SLIP_ESC_ESC=0xDD;

// CRC-8 0x07
uint8_t crc8_0x07(const uint8_t* d, size_t n, uint8_t init=0x00){
  uint8_t c=init;
  for(size_t i=0;i<n;i++){ c^=d[i]; for(uint8_t k=0;k<8;k++) c=(c&0x80)? (uint8_t)((c<<1)^0x07):(uint8_t)(c<<1); }
  return c;
}

// UART helpers
inline bool uart_avail(){ return uart_is_readable(UART_ID); }
inline uint8_t uart_readb(){ return (uint8_t)uart_getc(UART_ID); }
inline void uart_writeb(uint8_t b){ uart_putc_raw(UART_ID,(char)b); }

// SLIP write
void slip_write_byte(uint8_t b){
  if (b==SLIP_END){ uart_writeb(SLIP_ESC); uart_writeb(SLIP_ESC_END); }
  else if (b==SLIP_ESC){ uart_writeb(SLIP_ESC); uart_writeb(SLIP_ESC_ESC); }
  else uart_writeb(b);
}
void slip_write_frame(const uint8_t* data, size_t n){
  uart_writeb(SLIP_END);
  for(size_t i=0;i<n;i++) slip_write_byte(data[i]);
  uart_writeb(SLIP_END);
}

// Estado simple
volatile ActuationCmdV1 g_last_cmd{};
volatile uint32_t g_last_cmd_us = 0;
volatile uint16_t g_frame_ctr   = 0;

// SLIP RX parser de comandos
struct SlipRx {
  static const size_t MAX=128;
  uint8_t buf[MAX]; size_t len=0;
  bool in_frame=false, esc=false;
} RX;

void on_cmd_frame(uint8_t* buf, size_t n){
  if (n!=ACT_SIZE) return;
  ActuationCmdV1 c; memcpy(&c, buf, ACT_SIZE);
  if (c.sof!=ACT_SOF || c.version!=PROTO_VERSION || c.len!=(ACT_SIZE-1)) return;
  uint8_t calc = crc8_0x07((const uint8_t*)&c, ACT_SIZE-1, 0x00);
  if (calc!=c.crc8) return;

  // latch
  uint32_t irq=save_and_disable_interrupts();
  memcpy((void*)&g_last_cmd, &c, sizeof(c));
  g_last_cmd_us = (uint32_t)to_us_since_boot(get_absolute_time());
  restore_interrupts(irq);

  // Log liviano
  static uint32_t lastMs=0; uint32_t now=millis();
  if (now-lastMs>500){
    lastMs=now;
    Serial.printf("[RP2040] CMD seq=%u tgt_q15=%d mode=%u\n", c.seq, c.tgt_q15, c.mode);
  }
}

void slip_feed(uint8_t b){
  if (b==SLIP_END){
    if (RX.in_frame && RX.len>=ACT_SIZE) on_cmd_frame(RX.buf, RX.len);
    RX.len=0; RX.in_frame=true; RX.esc=false; return;
  }
  if (!RX.in_frame) return;
  if (RX.esc){
    if (b==SLIP_ESC_END) b=SLIP_END; else if (b==SLIP_ESC_ESC) b=SLIP_ESC;
    RX.esc=false;
  } else if (b==SLIP_ESC){ RX.esc=true; return; }
  if (RX.len<SlipRx::MAX) RX.buf[RX.len++]=b;
}

// Telemetría de prueba (simulada)
int16_t float_to_q15_turns(float turns){ return (int16_t)lrintf(turns*32768.0f); }
int16_t g_to_q14(float g){ return (int16_t)lrintf(g*16384.0f); }
int16_t gyro_to_q15(float rads, float maxr){ return (int16_t)lrintf((rads/maxr)*32768.0f); }

void send_telemetry(){
  TelemetryFrameV1 f{};
  f.sof=TEL_SOF; f.version=PROTO_VERSION; f.len=TEL_LEN_FIELD;
  static uint8_t seq=0; f.seq=seq++;
  f.t_us = (uint32_t)to_us_since_boot(get_absolute_time());
  f.flags = 0;
  f.sample_hz = 1000;
  // Simulamos un “encoder” senoidal en vueltas -> q15
  float t = millis()/1000.0f;
  float turns = 0.25f * sinf(2*PI*0.5f*t); // ±0.25 vuelta
  f.enc_q15 = float_to_q15_turns(turns);

  // IMU fake
  f.mpu_ax_q14 = g_to_q14(0.0f);
  f.mpu_ay_q14 = g_to_q14(0.0f);
  f.mpu_az_q14 = g_to_q14(1.0f);
  f.mpu_gx_q14 = gyro_to_q15(0.0f, 4.3633f);
  f.mpu_gy_q14 = gyro_to_q15(0.0f, 4.3633f);
  f.mpu_gz_q14 = gyro_to_q15(0.0f, 4.3633f);

  f.err_code=0; f.frame_ctr = g_frame_ctr++;
  // reserved: i_peak A *1024 (fake)
  int16_t ipeak_q = (int16_t)lrintf(0.5f * 1024.0f);
  f.reserved[0] = (uint8_t)(ipeak_q & 0xFF);
  f.reserved[1] = (uint8_t)((ipeak_q>>8) & 0xFF);
  f.reserved[2] = 0; f.reserved[3]=0; f.reserved[4]=0;

  f.crc8 = crc8_0x07((const uint8_t*)&f, TEL_SIZE-1, 0x00);

  slip_write_frame((const uint8_t*)&f, TEL_SIZE);
}

void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[RP2040] UART SLIP Device");

  uart_init(UART_ID, UART_BAUD);
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
  uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
  uart_set_fifo_enabled(UART_ID, true);

  Serial.println("[RP2040] UART0 en GP12/GP13 @2Mbps listo.");
}

void loop(){
  // 1) RX comandos
  while (uart_avail()){
    slip_feed(uart_readb());
  }
  // 2) TX telemetría (1 kHz aprox)
  static uint32_t last=0; uint32_t now=micros();
  if (now - last >= 1000){
    last = now;
    send_telemetry();
  }
}