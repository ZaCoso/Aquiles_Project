// ========= ESP32: Master UART (envía CMD, recibe TEL) =========
#include <Arduino.h>

// ==== Pines UART ESP32  ====
// OJO: Serial1.begin(baud, config, RX, TX)
#define ESP32_UART_RX 15   // RX del ESP32  (conectado al TX del RP2040/2350 GP12)
#define ESP32_UART_TX 6    // TX del ESP32  (conectado al RX del RP2040/2350 GP13)
#define ESP32_BAUD   2000000

// LED “heartbeat” (ajusta si tu placa usa otro)
#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

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

// SLIP
static const uint8_t SLIP_END=0xC0, SLIP_ESC=0xDB, SLIP_ESC_END=0xDC, SLIP_ESC_ESC=0xDD;

// CRC-8 polinomio 0x07
uint8_t crc8_0x07(const uint8_t* d, size_t n, uint8_t init=0x00){
  uint8_t c=init;
  for(size_t i=0;i<n;i++){ c^=d[i]; for(uint8_t k=0;k<8;k++) c=(c&0x80)? (uint8_t)((c<<1)^0x07):(uint8_t)(c<<1); }
  return c;
}

// SLIP write
void slip_write_byte(uint8_t b){
  if (b==SLIP_END){ Serial1.write(SLIP_ESC); Serial1.write(SLIP_ESC_END); }
  else if (b==SLIP_ESC){ Serial1.write(SLIP_ESC); Serial1.write(SLIP_ESC_ESC); }
  else Serial1.write(b);
}
void slip_write_frame(const uint8_t* data, size_t n){
  Serial1.write(SLIP_END);
  for(size_t i=0;i<n;i++) slip_write_byte(data[i]);
  Serial1.write(SLIP_END);
}

// SLIP RX parser simple para Telemetry
struct SlipRx {
  static const size_t MAX=128;
  uint8_t buf[MAX]; size_t len=0;
  bool in_frame=false, esc=false;
};
SlipRx g_slip_rx;

void on_tel_frame(uint8_t* buf, size_t n){
  if (n!=TEL_SIZE) return;
  TelemetryFrameV1 f; memcpy(&f, buf, TEL_SIZE);
  if (f.sof!=TEL_SOF || f.version!=PROTO_VERSION || f.len!=TEL_LEN_FIELD) return;
  uint8_t calc = crc8_0x07((const uint8_t*)&f, TEL_SIZE-1, 0x00);
  if (calc != f.crc8) return;

  // Log RX limitado
  static uint32_t lastMs=0; uint32_t now=millis();
  if (now-lastMs>200){
    lastMs=now;
    Serial.printf("[ESP32] RX TEL  seq=%u enc_q15=%d flags=0x%04X frame=%u\n",
                  f.seq, f.enc_q15, f.flags, f.frame_ctr);
  }
}

void slip_feed(uint8_t b){
  if (b==SLIP_END){
    if (g_slip_rx.in_frame && g_slip_rx.len>=TEL_SIZE)
      on_tel_frame(g_slip_rx.buf, g_slip_rx.len);
    g_slip_rx.len=0; g_slip_rx.in_frame=true; g_slip_rx.esc=false; return;
  }
  if (!g_slip_rx.in_frame) return;
  if (g_slip_rx.esc){ 
    if (b==SLIP_ESC_END) b=SLIP_END; else if (b==SLIP_ESC_ESC) b=SLIP_ESC;
    g_slip_rx.esc=false;
  } else if (b==SLIP_ESC){ g_slip_rx.esc=true; return; }
  if (g_slip_rx.len<SlipRx::MAX) g_slip_rx.buf[g_slip_rx.len++]=b;
}

// ======= ENVÍO =======
int16_t vel_to_q15(float vel_norm){ return (int16_t)lrintf(vel_norm * 32767.0f); }

void send_cmd_tick(){
  static uint8_t seq=0;
  static uint32_t t0=millis();
  float t=(millis()-t0)/1000.0f;
  float vel_norm = sinf(2.0f*PI*0.5f * t); // 0.5 Hz

  ActuationCmdV1 c{};
  c.sof=ACT_SOF; c.version=PROTO_VERSION; c.len=ACT_SIZE-1; c.seq=seq++;
  c.mode=2; // VELOCITY
  c.tgt_q15 = vel_to_q15(vel_norm);
  c.kp_q15=0; c.kd_q15=0; c.limits=0; c.rsv0=0; c.rsv1=0;
  c.crc8 = crc8_0x07((const uint8_t*)&c, ACT_SIZE-1, 0x00);

  slip_write_frame((const uint8_t*)&c, ACT_SIZE);

  // --------- LOG de envío limitado ---------
  static uint32_t lastPrint=0; uint32_t now=millis();
  if (now - lastPrint >= 200) {
    lastPrint = now;
    Serial.printf("[ESP32] TX CMD  seq=%u tgt_q15=%d vel_norm=%.3f\n",
                  c.seq, c.tgt_q15, vel_norm);
  }
}

// ======= SETUP / LOOP =======
void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("\n[ESP32] UART SLIP Master");

  Serial1.begin(ESP32_BAUD, SERIAL_8N1, ESP32_UART_RX, ESP32_UART_TX);
  Serial.printf("[ESP32] Serial1 @2Mbps  RX=%d  TX=%d\n", ESP32_UART_RX, ESP32_UART_TX);
}

void loop(){
  // 1) RX de Telemetry
  while (Serial1.available()){
    slip_feed((uint8_t)Serial1.read());
  }

  // 2) TX de Command (1 kHz aprox)
  static uint32_t last=0; uint32_t now=micros();
  if (now - last >= 1000){
    last = now;
    send_cmd_tick();
  }

  // 3) Heartbeat del LED cada ~500 ms (indica que estamos activos enviando)
  static uint32_t lastBlink=0; static bool led=0;
  if (millis() - lastBlink >= 500){
    lastBlink = millis();
    led = !led;
    digitalWrite(LED_BUILTIN, led);
  }
}