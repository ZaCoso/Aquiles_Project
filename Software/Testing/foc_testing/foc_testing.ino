#include <SimpleFOC.h>

#define AH_PIN 32
#define AL_PIN 33
#define BH_PIN 25
#define BL_PIN 26
#define CH_PIN 27
#define CL_PIN 14
#define EN_PIN 15

BLDCMotor motor = BLDCMotor(7);
BLDCDriver6PWM driver = BLDCDriver6PWM(AH_PIN, AL_PIN, BH_PIN, BL_PIN, CH_PIN, CL_PIN, EN_PIN);

void setup() {
  Serial.begin(115200);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  driver.voltage_power_supply = 12;
  driver.init();

  motor.linkDriver(&driver);
  motor.voltage_limit = 3;
  motor.controller = MotionControlType::torque;
  motor.foc_modulation = FOCModulationType::SinePWM;

  motor.init();

  // No initFOC, porque no hay sensor
}

void loop() {
  motor.move(0.5);  // Debería generar PWM en las 6 salidas
}