#include <Arduino.h>
#include "esp32-hal-ledc.h"   // <-- clave para que estén las LEDC APIs

#define BUZZER_PIN     11      // IO11 del ESP32-S3
#define LEDC_CHANNEL   0
#define LEDC_RES_BITS  10
#define LEDC_BASE_HZ   2000

void setup() {
  ledcSetup(LEDC_CHANNEL, LEDC_BASE_HZ, LEDC_RES_BITS);
  ledcAttachPin(BUZZER_PIN, LEDC_CHANNEL);
}

void beep(int freq, int ms) {
  ledcWriteTone(LEDC_CHANNEL, freq);
  delay(ms);
  ledcWriteTone(LEDC_CHANNEL, 0);
}

void loop() {
  beep(1000, 200);
  delay(300);
  beep(1500, 600);
  delay(1000);
}