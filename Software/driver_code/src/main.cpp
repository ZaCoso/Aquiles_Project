#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>
#include <AS5048A.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SimpleFOC.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"


#define RP_SPI   spi0
#define PIN_SCK  2   // SCK0
#define PIN_MOSI 3   // MOSI0 (TX del RP2040)
#define PIN_MISO 4   // MISO0 (RX del RP2040)
#define PIN_CS   1   // CSn0

AS5600 encoder_i2c;    

Adafruit_MPU6050 mpu;

// --- Definición de hardware ---
BLDCMotor motor = BLDCMotor(7);                  // Motor con 7 pares de polos
BLDCDriver3PWM driver = BLDCDriver3PWM(5, 6, 7); // Pines PWM para el driver
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C); // Sensor AS5600 por I2C
AS5048A encoder_spi(5);
// --- CRC helper ---
uint8_t crc8_sum(const uint8_t* d, size_t n) {
  uint16_t s = 0;
  for (size_t i = 0; i < n; ++i) s += d[i];
  return (uint8_t)(s & 0xFF);
}

// --- Inicialización SPI Slave ---
void init_spi_slave() {
  gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_CS,   GPIO_FUNC_SPI);

  spi_init(RP_SPI, 1000000);
  spi_set_slave(RP_SPI, true);
  spi_set_format(RP_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

  // Preload TX FIFO
  for (int i = 0; i < 8; ++i) {
    while (!spi_is_writable(RP_SPI)) { tight_loop_contents(); }
    spi_get_hw(RP_SPI)->dr = 0x55;
  }
}

// --- Lectura bloqueante de un byte SPI ---
uint8_t spi_read_byte() {
  while (!spi_is_readable(RP_SPI)) { tight_loop_contents(); }
  return (uint8_t)spi_get_hw(RP_SPI)->dr;
}

// --- Procesa un paquete SPI ---
bool handle_spi_packet() {
  // Esperar SOF
  uint8_t b = spi_read_byte();
  if (b != 0xAA) return false;

  // Leer LEN
  uint8_t len = spi_read_byte();
  if (len != 6) {
    for (int i = 0; i < len + 1; ++i) (void)spi_read_byte();
    return false;
  }

  // Leer payload
  uint8_t payload[6];
  for (int i = 0; i < 6; ++i) payload[i] = spi_read_byte();

  // Leer CRC
  uint8_t crc = spi_read_byte();

  // Validar CRC
  if (crc != crc8_sum(payload, 6)) {
    Serial1.println("CRC invalido");
    return false;
  }

  // Reconstruir 3 enteros
  int16_t v1 = (int16_t)(payload[0] | (payload[1] << 8));
  int16_t v2 = (int16_t)(payload[2] | (payload[3] << 8));
  int16_t v3 = (int16_t)(payload[4] | (payload[5] << 8));

  Serial1.print("OK: ");
  Serial1.print(v1); Serial1.print(", ");
  Serial1.print(v2); Serial1.print(", ");
  Serial1.println(v3);

  // Encolar respuesta (eco +1)
  for (int i = 0; i < 6; ++i) {
    while (!spi_is_writable(RP_SPI)) {}
    spi_get_hw(RP_SPI)->dr = payload[i] + 1;
  }

  return true;
}
float as5048aReadDegrees()
{
  // La mayoría de forks exponen getRawRotation(); si tu copia usa otro nombre, cámbialo aquí.
  const uint16_t raw = encoder_spi.getRawRotation(); // 0..16383
  return (raw * 360.0f) / 16384.0f;
}
// --- Función para inicializar SimpleFOC ---
void initSimpleFOC() {
  // Inicializar sensor
  sensor.init();
  motor.linkSensor(&sensor);

  // Configurar driver
  driver.voltage_power_supply = 5;   // Voltaje de alimentación del driver
  driver.init();
  motor.linkDriver(&driver);

  // Seleccionar tipo de control
  motor.controller = MotionControlType::angle;

  // Inicializar motor y FOC
  motor.init();
  motor.initFOC();

  Serial.println("Motor con FOC inicializado!");
}

// --- Función para mover el motor a un ángulo específico ---
void MotorAngMove(float angulo_rad) {
  motor.loopFOC();        
  motor.move(angulo_rad); 
}

// --- Función para mover el motor en velocidad ---
void MotorVelMove(float vel) {
  motor.loopFOC();
  motor.move(vel); 
}


// --- Función para leer y mostrar datos ---
void readMPU6050() {
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

  Serial.print("Temperatura: ");
  Serial.print(temp.temperature);
  Serial.println(" *C");

  Serial.println("---------------------------");
}

void encoder_5600(){
  long angle_raw = encoder_i2c.readAngle();   
  float degrees = (angle_raw / 4096.0) * 360.0;
  int magnitude = encoder_i2c.readMagnitude();    
  Serial.print("Raw: "); Serial.print(angle_raw);
  Serial.print("\tDeg: "); Serial.print(degrees, 3);
  Serial.print("\tMag: "); Serial.println(magnitude);
}
void encoder_5048(){}
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
  while (!Serial) { delay(10); }
  Wire.begin();  
  // mpu.begin();       
  encoder_init();
  init_spi_slave();
  // initSimpleFOC();
}

void loop() {


  delay(100);
}