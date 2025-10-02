#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>
#include <AS5048A.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

AS5600 encoder_i2c;    
AS5048A encoder_spi(5);  // pin CS = 5
Adafruit_MPU6050 mpu;

// --- Función para inicializar el MPU6050 ---
void initMPU6050() {
  if (!mpu.begin()) {
    Serial.println("No se encontró MPU6050. Revisa conexiones!");
    while (1) { delay(10); }
  }
  Serial.println("MPU6050 detectado!");

  // Configuración opcional
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);
}
// --- Función para leer y mostrar datos ---
void leerMPU6050() {
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
}
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Wire.begin();          // inicializa I2C
  encoder_init();
}

void loop() {
  encoder_5600();

  delay(100);
}