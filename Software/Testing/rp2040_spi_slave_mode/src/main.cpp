// RP2040-Zero SPI Slave (SPI0 en GPIO2..5) - Arduino Philhower
#include <Arduino.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define RP_SPI   spi0
#define PIN_SCK  2   // SCK0
#define PIN_MOSI 3   // MOSI0 (TX del RP2040)
#define PIN_MISO 4   // MISO0 (RX del RP2040)
#define PIN_CS   1   // CSn0

static inline uint8_t crc8_sum(const uint8_t* d, size_t n) {
  uint16_t s = 0; for (size_t i = 0; i < n; ++i) s += d[i];
  return (uint8_t)(s & 0xFF);
}

void setup() {
  Serial1.begin(115200);

  // Función SPI en pines seleccionados
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);

  // Inicializar SPI como SLAVE, 8 bits, modo 0, MSB
  spi_init(RP_SPI, 1000000);          // en slave manda el master
  spi_set_slave(RP_SPI, true);
  spi_set_format(RP_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  // Precargar TX FIFO (opcional) para no devolver 0x00
  for (int i = 0; i < 8; ++i) {
    while (!spi_is_writable(RP_SPI)) { tight_loop_contents(); }
    spi_get_hw(RP_SPI)->dr = 0x55;
  }

  Serial1.println("RP2040-Zero SPI Slave listo (GPIO2-5).");
}

static uint8_t read_byte_blocking() {
  while (!spi_is_readable(RP_SPI)) { tight_loop_contents(); }
  return (uint8_t)spi_get_hw(RP_SPI)->dr;
}

void loop() {
  // Esperar SOF
  uint8_t b = read_byte_blocking();
  if (b != 0xAA) return;

  // Leer LEN
  uint8_t len = read_byte_blocking();
  if (len != 6) {
    // Drenar payload+CRC si vino algo raro
    for (int i = 0; i < len + 1; ++i) (void)read_byte_blocking();
    return;
  }

  // Leer payload (3 x int16 LE)
  uint8_t payload[6];
  for (int i = 0; i < 6; ++i) payload[i] = read_byte_blocking();

  // Leer CRC
  uint8_t crc = read_byte_blocking();

  // Verificar CRC
  if (crc != crc8_sum(payload, 6)) {
    Serial1.println("CRC invalido");
    return;
  }
  
  // Reconstruir
  int16_t v1 = (int16_t)(payload[0] | (payload[1] << 8));
  int16_t v2 = (int16_t)(payload[2] | (payload[3] << 8));
  int16_t v3 = (int16_t)(payload[4] | (payload[5] << 8));

  Serial1.print("OK: ");
  Serial1.print(v1); Serial1.print(", ");
  Serial1.print(v2); Serial1.print(", ");
  Serial1.println(v3);

  // (Opcional) Responder algo: eco+1 (se irá clockeando en la próxima transacción)
  for (int i = 0; i < 6; ++i) {
    while (!spi_is_writable(RP_SPI)) {}
    spi_get_hw(RP_SPI)->dr = payload[i] + 1;
  }
}