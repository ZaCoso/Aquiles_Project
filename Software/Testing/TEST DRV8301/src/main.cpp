#include <Arduino.h>
#include <SimpleFOC.h>

// Pines PWM compatibles con slices correctos
#define AH_PIN 0
#define AL_PIN 2
#define BH_PIN 4
#define BL_PIN 6
#define CH_PIN 8
#define CL_PIN 10
#define EN_PIN 29  // Pin para EN_GATE del DRV8301

BLDCMotor motor = BLDCMotor(7);  // Ajustá la cantidad de pares de polos
BLDCDriver6PWM driver = BLDCDriver6PWM(AH_PIN, AL_PIN, BH_PIN, BL_PIN, CH_PIN, CL_PIN, EN_PIN);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== SimpleFOC + DRV8301 (6PWM RP2040) ===");

  // Habilitar DRV8301
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);

  // Configurar driver
  driver.voltage_power_supply = 12.0;
  driver.pwm_frequency = 25000;  // 25kHz ideal para DRV8301
  if (!driver.init()) {
    Serial.println("❌ Error al inicializar driver");
    while (1);
  }
  Serial.println("✅ Driver listo");

  // Configurar motor
  motor.linkDriver(&driver);
  motor.voltage_limit = 6.0;
  motor.controller = MotionControlType::torque;
  motor.foc_modulation = FOCModulationType::SinePWM;

  motor.init();
  Serial.println("✅ Motor listo");
}

void loop() {
  // Torque en open loop (sin sensor)
  motor.move(0.6);  // Aumentá si no se mueve
  delay(2000);
  motor.move(-0.6);
  delay(2000);
}