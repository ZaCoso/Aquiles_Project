#include <Arduino.h>

const int escPin = 18; // Connect ESC signal wire
const int freq = 50;   // 50 Hz for ESC/servo signal
const int resolution = 16; // 16-bit PWM
const int sweepAngle = 70; // ±70 degrees

void setup() {
  ledcSetup(0, freq, resolution); // Channel 0
  ledcAttachPin(escPin, 0);

  // Arm ESC
  uint32_t minPWM = map(1000, 1000, 2000, 0, 65535); // 0°
  ledcWrite(0, minPWM);
  delay(2000); // Wait 2 seconds for ESC to arm
}

void loop() {
  // Sweep right
  for (int angle = -sweepAngle; angle <= sweepAngle; angle += 1) {
    int pulse = map(angle, -sweepAngle, sweepAngle, 1000, 2000); // µs
    uint32_t pwm = map(pulse, 1000, 2000, 0, 65535);
    ledcWrite(0, pwm);
    delay(20); // speed of sweep
  }

  // Sweep left
  for (int angle = sweepAngle; angle >= -sweepAngle; angle -= 1) {
    int pulse = map(angle, -sweepAngle, sweepAngle, 1000, 2000); // µs
    uint32_t pwm = map(pulse, 1000, 2000, 0, 65535);
    ledcWrite(0, pwm);
    delay(20); // speed of sweep
  }
}