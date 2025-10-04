#include <Arduino.h>
#include <SimpleFOC.h>

// ------------------- Pines driver 6PWM (RP2040) -------------------
// Fase A
#define INHA_PIN 0   // GP0 -> HIN1
#define INLA_PIN 1   // GP1 -> LIN1
// Fase B
#define INHB_PIN 2   // GP2 -> HIN2
#define INLB_PIN 3   // GP3 -> LIN2
// Fase C
#define INHC_PIN 4   // GP4 -> HIN3
#define INLC_PIN 5   // GP5 -> LIN3

// ------------------- SPI AS5048A (RP2040 SPI0 por defecto) -------------------
// SPI0: SCK=GP18, MOSI=GP19, MISO=GP16 (no cambies estos a menos que uses SPI1)
// Chip Select del sensor:
#define AS5048A_CS 17  // GP17 -> CS

// ------------------- Parámetros del motor -------------------
#define POLE_PAIRS 7

// ------------------- Instancias SimpleFOC -------------------
BLDCMotor motor = BLDCMotor(POLE_PAIRS);
BLDCDriver6PWM driver = BLDCDriver6PWM(INHA_PIN, INLA_PIN, INHB_PIN, INLB_PIN, INHC_PIN, INLC_PIN);

// Sensor AS5048A nativo de SimpleFOC (SPI modo 1)
MagneticSensorSPI sensor = MagneticSensorSPI(AS5048_SPI, AS5048A_CS);

void setup() {
  Serial.begin(115200);
  delay(500);

  // ---- Inicializar sensor (SPI, modo correcto para AS5048A) ----
  sensor.init();
  motor.linkSensor(&sensor);

  // ---- Configurar driver 6PWM ----
  driver.voltage_power_supply = 12.0;   // Alimentación
  driver.dead_zone = 0.04;              // tu valor
  driver.pwm_frequency = 1000;          // tu valor
  driver.init();

  // ---- Vincular driver <-> motor ----
  motor.linkDriver(&driver);

  // Límites y control
  motor.voltage_limit = 6.0;            // empieza conservador; sube si hace falta
  motor.velocity_limit = 100.0;         // rad/s máx (ajusta a tu setup)
  motor.controller = MotionControlType::velocity;  // lazo cerrado de velocidad

  // PID de velocidad (ajústalo a tu hardware)
  motor.PID_velocity.P = 0.2;
  motor.PID_velocity.I = 5.0;
  motor.PID_velocity.D = 0.0;
  motor.LPF_velocity.Tf = 0.01;

  // Inicializa FOC (detecta sentido/offset del sensor)
  // Si gira mal o vibra, prueba motor.initFOC(Direction::CCW) o ::CW
  motor.init();
  motor.initFOC();

  // Objetivo de velocidad (rad/s). Cambia por Serial si quieres.
  motor.target = 20.0;

  Serial.println("SimpleFOC + AS5048A listo.");
}

void loop() {
  // calcula FOC (lee sensor, cierra lazo de corriente/ang.)
  motor.loopFOC();
  // mueve según el controlador elegido (velocidad en este caso)
  motor.move();

  // (opcional) monitoreo simple
  // static uint32_t t=0;
  // if (millis()-t>200) { t=millis();
  //   Serial.print("angle "); Serial.print(sensor.getAngle());
  //   Serial.print("  vel "); Serial.println(motor.shaft_velocity);
  // }
}	