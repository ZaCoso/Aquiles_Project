#include <Arduino.h>
#include <Wire.h>
#include <AS5600.h>
#include <AS5048A.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SimpleFOC.h>


AS5600 encoder_i2c;    
AS5048A encoder_spi(5);  // pin CS = 5
Adafruit_MPU6050 mpu;

// --- Definición de hardware ---
BLDCMotor motor = BLDCMotor(7);                  // Motor con 7 pares de polos
BLDCDriver3PWM driver = BLDCDriver3PWM(5, 6, 7); // Pines PWM para el driver
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C); // Sensor AS5600 por I2C

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
}
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  Wire.begin();  
  mpu.begin();       
  encoder_init();
  initSimpleFOC();
}

void loop() {


  delay(100);
}