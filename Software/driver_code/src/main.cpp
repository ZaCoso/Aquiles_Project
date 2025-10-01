#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>

AS5600 encoder;    

void sensor_measure(){
  long angle_raw = encoder.readAngle();   
  float degrees = (angle_raw / 4096.0) * 360.0;
  int magnitude = encoder.readMagnitude();    
  Serial.print("Raw: "); Serial.print(angle_raw);
  Serial.print("\tDeg: "); Serial.print(degrees, 3);
  Serial.print("\tMag: "); Serial.println(magnitude);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Wire.begin();          // inicializa I2C
  if (!encoder.begin()) {
    Serial.println("AS5600 no encontrado!");
    while (1) delay(200);
  }
  Serial.println("AS5600 listo.");
}

void loop() {


  delay(100);
}