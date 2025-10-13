// RP2040 Zero - SPI1 SLAVE diagnóstico (SCK=10, MOSI=11, MISO=12, CS=13)
#include <Arduino.h>
#include "hardware/spi.h"

static const uint PIN_SCK  = 10;
static const uint PIN_MOSI = 11;
static const uint PIN_MISO = 12;
static const uint PIN_CS   = 13;

static uint32_t g_rx_count = 0;
static uint8_t  g_first = 0, g_last = 0;
static bool     g_first_seen = false;

void preflight_gpio_check() {
  Serial.println("[RP] === Preflight 1s: GPIO check (antes de SPI) ===");
  pinMode(PIN_CS,  INPUT_PULLUP);
  pinMode(PIN_SCK, INPUT);
  uint32_t edges=0;
  int last = digitalRead(PIN_SCK);
  uint32_t t0 = millis();
  while (millis() - t0 < 1000) {
    int cs = digitalRead(PIN_CS);
    int s  = digitalRead(PIN_SCK);
    // sólo contamos flancos de SCK cuando CS está LOW (seleccionado)
    if (cs==LOW && s!=last) { edges++; last=s; }
  }
  Serial.printf("[RP] CS=%s durante test (observado); flancos SCK (CS=LOW) en 1s: %lu\n",
                digitalRead(PIN_CS)?"HIGH":"LOW", edges);
  if (edges==0) {
    Serial.println("[RP][ALERTA] No se observaron flancos SCK con CS=LOW. Revise SCK o CS.");
  }
}

void setup_spi_slave() {
  // Todos los pines a función SPI (¡incluido CS!)
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);

  spi_init(spi1, 1000*1000);
  spi_set_slave(spi1, true);
  spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  // Pre-cargar primer byte para que el master lea 0x5A como seed
  while (!spi_is_writable(spi1)) {}
  spi_get_hw(spi1)->dr = 0x5A;

  Serial.println("[RP] SPI1 slave armado (MODE0, 1MHz, seed=0x5A).");
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("\n[RP] === RP2040 Zero - Diagnóstico SPI SLAVE ===");
  preflight_gpio_check();
  setup_spi_slave();
}

void loop() {
  static uint32_t t0 = millis();
  // Cuando el master envía un byte, respondemos eco ^0xA5
  if (spi_is_readable(spi1)) {
    uint8_t in = (uint8_t)spi_get_hw(spi1)->dr;
    uint8_t out = in ^ 0xA5;
    while (!spi_is_writable(spi1)) {}
    spi_get_hw(spi1)->dr = out;

    if (!g_first_seen) { g_first = in; g_first_seen = true; }
    g_last = in;
    g_rx_count++;
  }

  // Status cada ~500 ms
  if (millis() - t0 >= 500) {
    t0 = millis();
    Serial.printf("[RP] RX bytes=%lu, first=%02X, last=%02X\n",
                  (unsigned long)g_rx_count, g_first, g_last);

    if (g_rx_count==0) {
      Serial.println("[RP][WARN] Sin bytes recibidos: ¿CS en función SPI? ¿SCK llega al pin correcto?");
    }
  }
}