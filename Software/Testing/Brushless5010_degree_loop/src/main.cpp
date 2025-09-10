#include <Arduino.h>
#include <iostream>
#include <cstdlib>

// --- CONFIG ---
const int PIN_ESC = 18;
const int CANAL = 0;
const int FREQ = 50;        // 50 Hz señal de servo/ESC
const int RES = 16;         // 16 bits

// Señales típicas de servo/ESC
const int PULSE_NEUTRO = 1000; // motor parado
const int PULSE_DERECHA = 1300; // velocidad baja derecha
const int PULSE_IZQUIERDA = 1300; // velocidad baja izquierda

// Calibración por tiempo
float K_MS_POR_GRADO = 12.0; // ms que tarda en girar 1° a esa velocidad

const int LIM_GRADOS = 70; // ±70°

// Convierte microsegundos a duty cycle
void setPulseUS(int microseconds) {
  int duty = (int)((microseconds / 20000.0) * ((1 << RES) - 1));
  ledcWrite(CANAL, duty);
}

void parar() { setPulseUS(PULSE_NEUTRO); }

// Gira por tiempo equivalente a grados
void irRelativoGrados(float grados) {
  if (grados == 0) return;

  bool derecha = (grados > 0);
  int pulso = derecha ? PULSE_DERECHA : PULSE_IZQUIERDA;
  unsigned long t = (unsigned long)(fabs(grados) * K_MS_POR_GRADO);

  setPulseUS(pulso);
  delay(t);
  parar();
  delay(50); // pequeño settle
}

// Llevar a un offset relativo desde centro (−70° a +70°)
void irAOffset(float offsetGrados) {
  if (offsetGrados > LIM_GRADOS)  offsetGrados = LIM_GRADOS;
  if (offsetGrados < -LIM_GRADOS) offsetGrados = -LIM_GRADOS;
  irRelativoGrados(offsetGrados);
}

void setup() {
  ledcSetup(CANAL, FREQ, RES);
  ledcAttachPin(PIN_ESC, CANAL);

  // Inicializar ESC en neutro
  setPulseUS(PULSE_NEUTRO);
  delay(3000); // esperar a que el ESC arme
}

void loop() {
  irAOffset(+LIM_GRADOS);  // +70°
  delay(800);
  irAOffset(-2 * LIM_GRADOS); // de +70° a -70°
  delay(800);
  irAOffset(+LIM_GRADOS); // volver al centro
  delay(1200);
}

// ----------------- MotorController -----------------
class MotorController {
private:
    float currentSpeed;
    float targetSpeed;

public:
    MotorController() : currentSpeed(0), targetSpeed(0) {}
    void setSpeed(float speed) { targetSpeed = speed; }
    void update() {
        float error = targetSpeed - currentSpeed;
        currentSpeed += error * 0.1; // ganancia simple tipo P
        std::cout << "Motor speed: " << currentSpeed << std::endl;
    }
    float getSpeed() { return currentSpeed; }
};

// ----------------- ProtectionPhase -----------------
class ProtectionPhase {
private:
    bool overCurrent;
    bool overTemp;

public:
    ProtectionPhase() : overCurrent(false), overTemp(false) {}
    void check(float current, float temperature) {
        overCurrent = current > 10.0;      // límite de corriente
        overTemp = temperature > 70.0;     // límite de temperatura

        if(overCurrent) std::cout << "Error: Overcurrent detected!" << std::endl;
        if(overTemp) std::cout << "Error: Overtemperature detected!" << std::endl;
    }
    bool hasError() { return overCurrent || overTemp; }
};

// ----------------- Sensor -----------------
class Sensor {
public:
    Sensor() {}
    float readCurrent() { return static_cast<float>(rand() % 15); }      // simulado
    float readTemperature() { return static_cast<float>(rand() % 100); } // simulado
};

// ----------------- StateMachine -----------------
enum class State { IDLE, RUNNING, ERROR };

class StateMachine {
private:
    State currentState;

public:
    StateMachine() : currentState(State::IDLE) {}
    void setState(State s) { currentState = s; }
    State getState() { return currentState; }
    void update(bool errorDetected) {
        if(errorDetected) {
            currentState = State::ERROR;
            std::cout << "State changed to ERROR" << std::endl;
        } else if(currentState == State::IDLE) {
            currentState = State::RUNNING;
            std::cout << "State changed to RUNNING" << std::endl;
        }
    }
};

// ----------------- Main -----------------
int main() {
    MotorController motor;
    ProtectionPhase protection;
    Sensor sensor;
    StateMachine state;

    for(int i=0; i<20; ++i) {  // ciclo principal simulado
        float current = sensor.readCurrent();
        float temp = sensor.readTemperature();

        protection.check(current, temp);

        state.update(protection.hasError());

        if(state.getState() == State::RUNNING) {
            motor.setSpeed(5.0); // velocidad deseada
        } else {
            motor.setSpeed(0);
        }

        motor.update();

        std::cout << "-------------------" << std::endl;
    }

    return 0;
}