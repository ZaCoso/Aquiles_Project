#include "pico/stdlib.h"
#include "Arduino.h"
#include "hardware/spi.h"

// Pines SPI0 por defecto en el Pico
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

void setup() {
  // Inicializar pines
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);

  // Inicializar S/-=[\]\
\]
PI0 en modo slave
  spi_init(spi0, 1000 * 1000);   // La freq acá no importa mucho, la marca el master
  spi_set_slave(spi0, true);

  // Formato: 8 bits, modo 0 (CPOL=0, CPHA=0)
  spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  Serial1.begin(115200); // Usar Serial1 (GPIO 4/5) o el que tengas libre
  Serial1.println("SPI Slave listo!");
}

void loop() {
  uint8_t rxbuf;
  // Bloquea hasta que llegue un byte
  if (spi_is_readable(spi0)) {
    rxbuf = (uint8_t)spi_get_hw(spi0)->dr;  // Leer del data register
    Serial1.print("Recibido: 0x");
    Serial1.println(rxbuf, HEX);

    // Mandar respuesta en el mismo ciclo (ej: eco + 1)
    uint8_t txval = rxbuf + 1;
    while (!spi_is_writable(spi0)) { tight_loop_contents(); }
    spi_get_hw(spi0)->dr = txval;
  }
}