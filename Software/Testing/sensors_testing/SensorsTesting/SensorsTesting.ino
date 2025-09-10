#include <Wire.h>
#include <MPU6050_light.h>
#include <AS5600.h>

#define SDA_PIN 21
#define SCL_PIN 22

MPU6050 mpu(Wire);   // ✅ Esta sí acepta "Wire"
AS5600 as5600;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(500);

  Serial.println("🔍 Inicializando sensores...");

  mpu.begin();
  Serial.println("✅ MPU6050 listo");

  as5600.begin();
  as5600.setDirection(AS5600_CLOCK_WISE);
  Serial.println("✅ AS5600 listo");

  mpu.calcOffsets();  // opcional pero mejora lectura
}

void loop() {
  mpu.update();

  Serial.print("Gyro X: ");
  Serial.print(mpu.getGyroX());
  Serial.print(" | Y: ");
  Serial.print(mpu.getGyroY());
  Serial.print(" | Z: ");
  Serial.print(mpu.getGyroZ());

  int angleRaw = as5600.readAngle();
  float degrees = angleRaw * 0.0879;
  Serial.print(" | Encoder: ");
  Serial.print(degrees, 2);
  Serial.println("°");

  delay(200);
}