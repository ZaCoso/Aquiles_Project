#include <SimpleFOC.h>

// Phase A
#define INHA_PIN 0   // GP0 → HIN1
#define INLA_PIN 1   // GP1 → LIN1

// Phase B
#define INHB_PIN 2   // GP2 → HIN2
#define INLB_PIN 3   // GP3 → LIN2

// Phase C
#define INHC_PIN 4   // GP4 → HIN3
#define INLC_PIN 5   // GP5 → LIN3

// Motor parameter
#define POLE_PAIRS 7   // Cambiar según tu motor real

// Define motor and driver instances
BLDCMotor motor = BLDCMotor(POLE_PAIRS);
BLDCDriver6PWM driver = BLDCDriver6PWM(INHA_PIN, INLA_PIN, INHB_PIN, INLB_PIN, INHC_PIN, INLC_PIN);

void setup() {
  Serial.begin(115200);

  // Configurar driver
  driver.voltage_power_supply = 12;   // Alimentación
  driver.dead_zone = 0.04;
  driver.pwm_frequency = 1000;
  driver.init();

  // Vincular motor y driver
  motor.linkDriver(&driver);

  // Control en lazo abierto de velocidad
  motor.controller = MotionControlType::velocity_openloop;
  motor.voltage_limit = 12;   // Voltaje seguro para pruebas

  // Inicializar motor
  motor.init();

  // Velocidad objetivo inicial
  motor.target = 100;   // [rad/s]
}

void loop() {
  motor.loopFOC();  // Necesario aunque sea open-loop
  motor.move();     // Ejecuta el movimiento
}
