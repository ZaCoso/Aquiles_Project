#include <SimpleFOC.h>

// Pines PWM según tu pinout
#define INU 1   // GP1 → fase U (LIN1)
#define INV 5   // GP5 → fase V (LIN2)
#define INW 3   // GP3 → fase W (LIN3)

// Pines de enable
#define EN1 0   // GP0 → SD1
#define EN2 4   // GP4 → SD2
#define EN3 2   // GP2 → SD3

// Motor y driver
BLDCMotor motor(7);                   // 7 pole pairs (cambia por el tuyo)
BLDCDriver3PWM driver(INU, INV, INW); 

void setup() {
  Serial.begin(115200);
  Serial.println("Test elemental IR2104 + RP2040");
  driver.pwm_frequency = 30000;

  // Enable pins
  pinMode(EN1, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(EN3, OUTPUT);
  digitalWrite(EN1, HIGH);
  digitalWrite(EN2, HIGH);
  digitalWrite(EN3, HIGH);

  // Driver simple
  driver.voltage_power_supply = 9.8;   // ajusta a tu fuente
  driver.voltage_limit = 8;           // bajo para test
  driver.init();
  motor.linkDriver(&driver);

  // Config motor: openloop voltage
  motor.controller = MotionControlType::velocity_openloop;
  motor.voltage_limit = 3;    // subí luego
  motor.init();



  Serial.println("Listo, escribí velocidad en consola");
}

void loop() {
  motor.loopFOC(); 

  // mueve en open-loop con target fijo
  motor.move();

  // lectura consola → setear velocidad
  if (Serial.available()) {
    String in = Serial.readStringUntil('\n');
    float val = in.toFloat();
    motor.target = val;   // velocidad en rad/s
    Serial.print("Nuevo target = ");
    Serial.println(val);
  }
}
