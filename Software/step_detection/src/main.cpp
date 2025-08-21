#include <Arduino.h>
#include "step_intention_sys.h"

StepIntentSystem stepSystem(25.0);  // Adjust threshold if needed
unsigned long lastTrigger = 0;
const unsigned long cooldown = 800; // ms

void setup() {
    Serial.begin(115200);
    stepSystem.begin();
}

void loop() {
    stepSystem.update();

    if (stepSystem.isStepIntended()) {
        unsigned long now = millis();
        if (now - lastTrigger > cooldown) {
            Serial.println("🚶 Step intention detected!");
            lastTrigger = now;
        }
        stepSystem.reset();
    }

    delay(20); // ~50 Hz
}