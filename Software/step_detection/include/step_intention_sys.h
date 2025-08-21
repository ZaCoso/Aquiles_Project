#ifndef STEP_INTENT_SYSTEM_H
#define STEP_INTENT_SYSTEM_H

#include <Wire.h>
#include <AS5600.h>

// ===================== Encoder Handler =====================
class EncoderHandler {
private:
    AS5600 encoder;
    float lastAngle;
    float angularVelocity;
    unsigned long lastTime;

public:
    EncoderHandler() {}

    void begin() {
        Wire.begin();
        encoder.begin();
        encoder.setDirection(AS5600_CLOCK_WISE);
        lastAngle = getAngleDeg();
        lastTime = millis();
        angularVelocity = 0;
    }

    void update() {
        float currentAngle = getAngleDeg();
        unsigned long now = millis();
        float dt = (now - lastTime) / 1000.0; // seconds

        float delta = currentAngle - lastAngle;

        // Handle wrap-around (360° ↔ 0°)
        if (delta > 180) delta -= 360;
        if (delta < -180) delta += 360;

        angularVelocity = delta / dt;

        lastAngle = currentAngle;
        lastTime = now;
    }

    float getAngularVelocity() {
        return angularVelocity;
    }

private:
    float getAngleDeg() {
        uint16_t raw = encoder.readAngle();           
        return raw * (360.0 / 4096.0);                // convert to degrees
    }
};

// ===================== Step Intent Detector =====================
class StepIntentDetector {
private:
    float threshold;
    bool triggered;

public:
    StepIntentDetector(float angularVelThreshold = 25.0) {
        threshold = angularVelThreshold;
        triggered = false;
    }

    void update(float angularVelocity) {
        if (!triggered && abs(angularVelocity) > threshold) {
            triggered = true;
        }
    }

    bool isTriggered() {
        return triggered;
    }

    void reset() {
        triggered = false;
    }
};

// ===================== Main System =====================
class StepIntentSystem {
private:
    EncoderHandler encoder;
    StepIntentDetector detector;

public:
    StepIntentSystem(float threshold = 25.0) : detector(threshold) {}

    void begin() {
        encoder.begin();
    }

    void update() {
        encoder.update();
        detector.update(encoder.getAngularVelocity());
    }

    bool isStepIntended() {
        return detector.isTriggered();
    }

    void reset() {
        detector.reset();
    }
};

#endif 