// RP2040 Zero - 3x ACS725LLCTR-50AB-T
// Pines ADC: GP27, GP28, GP29
// Corriente en amperios con calibración inicial y auto-calibración dinámica

const int PIN0 = 27;  // ADC1
const int PIN1 = 28;  // ADC2
const int PIN2 = 29;  // ADC3

// Configuración del sensor
const float ADC_RES = 4095.0f;     // 12 bits
const float VCC_NOM = 3.3f;        // VCC nominal
const float SENS_V_PER_A = 0.0264; // 26.4 mV/A @ 3.3V
const float COUNTS_PER_AMP = (SENS_V_PER_A * ADC_RES) / VCC_NOM;

// Filtro de corriente (IIR)
const float ALPHA_I = 0.1f;

// Auto-calibración
const float ZERO_THRESHOLD = 0.2f;   // A, rango para considerar “casi cero”
const int ZERO_HOLD_TIME = 2000;     // ms, tiempo que debe estar en cero
const float OFFSET_ADAPT_ALPHA = 0.001f; // rapidez de adaptación del offset

// Offset calibrado (en cuentas ADC)
float off0 = 2048, off1 = 2048, off2 = 2048;

// Estado del filtro
float i0_f = 0, i1_f = 0, i2_f = 0;

// Para auto-calibración
unsigned long zeroStart = 0;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  // Calibración inicial (asume sin carga al encender)
  long a0=0, a1=0, a2=0;
  for (int i=0; i<500; i++) {
    a0 += analogRead(PIN0);
    a1 += analogRead(PIN1);
    a2 += analogRead(PIN2);
    delayMicroseconds(500);
  }
  off0 = a0 / 500.0f;
  off1 = a1 / 500.0f;
  off2 = a2 / 500.0f;

  Serial.print("# Offset inicial: ");
  Serial.print(off0); Serial.print(", ");
  Serial.print(off1); Serial.print(", ");
  Serial.println(off2);
  Serial.println("# I0[A]\tI1[A]\tI2[A]");
}

void loop() {
  // Lectura ADC
  int r0 = analogRead(PIN0);
  int r1 = analogRead(PIN1);
  int r2 = analogRead(PIN2);

  // Corrientes (A)
  float i0 = (r0 - off0) / COUNTS_PER_AMP;
  float i1 = (r1 - off1) / COUNTS_PER_AMP;
  float i2 = (r2 - off2) / COUNTS_PER_AMP;

  // Filtro IIR
  i0_f = ALPHA_I * i0 + (1 - ALPHA_I) * i0_f;
  i1_f = ALPHA_I * i1 + (1 - ALPHA_I) * i1_f;
  i2_f = ALPHA_I * i2 + (1 - ALPHA_I) * i2_f;

  // --- AUTO-CALIBRACIÓN DINÁMICA ---
  if (fabs(i0_f) < ZERO_THRESHOLD &&
      fabs(i1_f) < ZERO_THRESHOLD &&
      fabs(i2_f) < ZERO_THRESHOLD) {
    // Si estamos cerca de cero
    if (zeroStart == 0) zeroStart = millis();
    if (millis() - zeroStart > ZERO_HOLD_TIME) {
      // Adaptar offsets lentamente hacia los valores actuales
      off0 = (1 - OFFSET_ADAPT_ALPHA) * off0 + OFFSET_ADAPT_ALPHA * r0;
      off1 = (1 - OFFSET_ADAPT_ALPHA) * off1 + OFFSET_ADAPT_ALPHA * r1;
      off2 = (1 - OFFSET_ADAPT_ALPHA) * off2 + OFFSET_ADAPT_ALPHA * r2;
    }
  } else {
    zeroStart = 0; // se canceló la condición de “casi cero”
  }

  // Enviar al Plotter
  Serial.print(i0_f, 3); Serial.print("\t");
  Serial.print(i1_f, 3); Serial.print("\t");
  Serial.println(i2_f, 3);

  delay(10); // ~100 Hz
}
