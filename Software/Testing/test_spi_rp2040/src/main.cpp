// RP2040 Zero - SPI1 SLAVE simple echo (pins: SCK=10, MOSI=11, MISO=12, CS=13)
#include <Arduino.h>
#include "hardware/spi.h"

static const uint PIN_SCK=10, PIN_MOSI=11, PIN_MISO=12, PIN_CS=13;

static inline uint32_t micros32(){ return (uint32_t)to_us_since_boot(get_absolute_time()); }

void setup() {
  Serial.begin(115200);

  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_IN); gpio_pull_up(PIN_CS);

  spi_init(spi1, 1000*1000);          // clk nominal (lo maneja el master)
  spi_set_slave(spi1, true);
  spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  // Semilla en TX FIFO para la 1ª lectura del master:
  while (!spi_is_writable(spi1)) {}
  spi_get_hw(spi1)->dr = 0x5A;

  Serial.println("[RP2040] SPI1 slave echo listo");
}

void loop() {
  // Cuando hay un byte recibido, ponemos en TX su eco ^0xA5
  if (spi_is_readable(spi1)) {
    uint8_t in = (uint8_t)spi_get_hw(spi1)->dr;
    uint8_t out = in ^ 0xA5;
    while (!spi_is_writable(spi1)) {}
    spi_get_hw(spi1)->dr = out;
    // (opcional debug)
    Serial.printf("rx=%02X tx=%02X\n", in, out);
  }
  static int last_cs = 1;
  int cs_now = gpio_get(PIN_CS);
  if (cs_now != last_cs) {
    Serial.printf("CS cambio: %s\n", cs_now ? "HIGH" : "LOW");
    last_cs = cs_now;
  }
}