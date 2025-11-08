/*
  Aquiles x1 – ESP32-S3 BLE mock telemetry (verbose command logging)
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <math.h>

#define DEVICE_NAME "Aquiles x1"

static const char* SERVICE_UUID = "6e400010-b5a3-f393-e0a9-e50e24dcca9e";
static const char* CMD_UUID     = "6e400020-b5a3-f393-e0a9-e50e24dcca9e";
static const char* IMU_L_UUID   = "6e400021-b5a3-f393-e0a9-e50e24dcca9e";
static const char* IMU_R_UUID   = "6e400022-b5a3-f393-e0a9-e50e24dcca9e";
static const char* ENC_L_UUID   = "6e400023-b5a3-f393-e0a9-e50e24dcca9e";
static const char* ENC_R_UUID   = "6e400024-b5a3-f393-e0a9-e50e24dcca9e";
static const char* FORCE_UUID   = "6e400025-b5a3-f393-e0a9-e50e24dcca9e";

BLEServer*         pServer   = nullptr;
BLEService*        pService  = nullptr;
BLECharacteristic* chCmd     = nullptr;
BLECharacteristic* chImuL    = nullptr;
BLECharacteristic* chImuR    = nullptr;
BLECharacteristic* chEncL    = nullptr;
BLECharacteristic* chEncR    = nullptr;
BLECharacteristic* chForce   = nullptr;

volatile bool deviceConnected = false;
volatile bool streamEnabled   = false;

struct Settings { uint8_t AID=50, RES=50, SEN=50; } settings;

// --- helpers ---
static inline void notifyBytes(BLECharacteristic* ch, const void* data, size_t len) {
  ch->setValue((uint8_t*)data, len);
  ch->notify();
}
static inline void notifyFloatArray(BLECharacteristic* ch, const float* data, size_t count) {
  notifyBytes(ch, data, count * sizeof(float));
}
static inline void notifyOneFloat(BLECharacteristic* ch, float value) {
  notifyBytes(ch, &value, sizeof(float));
}
static inline uint8_t clamp0100(int v) { return v<0?0:(v>100?100:v); }

// --- BLE callbacks ---
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected");
  }
  void onDisconnect(BLEServer* s) override {
    deviceConnected = false;
    streamEnabled = false;
    Serial.println("[BLE] Client disconnected");
    s->getAdvertising()->start();
  }
};

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String s = c->getValue();
    s.trim();
    if (s.length()==0) return;

    Serial.print("[BLE CMD RAW] ");
    Serial.println(s);

    // Basic triggers
    if (s == "1" || s.equalsIgnoreCase("ON")) {
      streamEnabled = true;
      Serial.println("[ACTION] Streaming ENABLED");
      return;
    }
    if (s.equalsIgnoreCase("OFF")) {
      streamEnabled = false;
      Serial.println("[ACTION] Streaming DISABLED");
      return;
    }

    // Parse CMD:VALUE
    int colon = s.indexOf(':');
    if (colon > 0) {
      String cmd = s.substring(0, colon); cmd.trim(); cmd.toUpperCase();
      String val = s.substring(colon + 1); val.trim();

      if (cmd == "AID") {
        settings.AID = clamp0100(val.toInt());
        Serial.printf("[CMD] AID = %u (Assistance)\n", settings.AID);
      }
      else if (cmd == "RES") {
        settings.RES = clamp0100(val.toInt());
        Serial.printf("[CMD] RES = %u (Responsiveness)\n", settings.RES);
      }
      else if (cmd == "SEN") {
        settings.SEN = clamp0100(val.toInt());
        Serial.printf("[CMD] SEN = %u (Sensitivity)\n", settings.SEN);
      }
      else if (cmd == "SPD") {
        Serial.printf("[CMD] SPD = %s (Mock speed)\n", val.c_str());
      }
      else if (cmd == "MODE") {
        Serial.printf("[CMD] MODE = %s (Mock mode)\n", val.c_str());
      }
      else if (cmd == "CAL") {
        Serial.println("[CMD] CAL request (Mock calibration)");
      }
      else if (cmd == "BAT") {
        Serial.println("[CMD] BAT request (Mock battery)");
      }
      else if (cmd == "STA") {
        Serial.println("[CMD] STA request (Mock status)");
      }
      else {
        Serial.printf("[CMD] Unknown key '%s' with value '%s'\n", cmd.c_str(), val.c_str());
      }
      return;
    }

    // Non-colon commands
    String up = s; up.toUpperCase();
    if (up == "CAL" || up == "BAT" || up == "STA") {
      Serial.printf("[CMD] %s (mock single-word)\n", up.c_str());
    } else {
      Serial.printf("[CMD] Unrecognized single token: '%s'\n", s.c_str());
    }
  }
};

// --- mock telemetry ---
float tphase = 0.0f;
static inline void genIMU(float out[6], float phase) {
  out[0] = 0.1f * sinf(tphase + phase);
  out[1] = 0.2f * cosf(tphase * 0.8f + phase);
  out[2] = 1.00f + 0.02f * sinf(tphase * 0.4f + phase);
  out[3] = 10.0f * sinf(tphase * 1.5f + phase);
  out[4] = 12.0f * cosf(tphase * 1.2f + phase);
  out[5] =  8.0f * sinf(tphase * 0.9f + phase);
}
static inline float genAngle(float off) { return 45.0f + 40.0f * sinf(tphase + off); }
static inline void genForce(float out[3]) {
  out[0] =  5.0f + 2.5f * sinf(tphase * 0.7f);
  out[1] = -1.5f + 2.0f * cosf(tphase * 0.6f);
  out[2] = 12.0f + 3.0f * sinf(tphase * 0.5f);
}

// --- timing ---
uint32_t lastIMUms=0, lastENCms=0, lastFORCEms=0;
const uint32_t IMU_PERIOD_MS=40, ENC_PERIOD_MS=50, FORCE_PERIOD_MS=50;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[Aquiles x1] ESP32 BLE Arduino – Verbose Command Logging");

  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(185);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  chCmd = pService->createCharacteristic(CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
  chCmd->setCallbacks(new CmdCallbacks());

  chImuL  = pService->createCharacteristic(IMU_L_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  chImuR  = pService->createCharacteristic(IMU_R_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  chEncL  = pService->createCharacteristic(ENC_L_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  chEncR  = pService->createCharacteristic(ENC_R_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  chForce = pService->createCharacteristic(FORCE_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  chImuL->addDescriptor(new BLE2902());
  chImuR->addDescriptor(new BLE2902());
  chEncL->addDescriptor(new BLE2902());
  chEncR->addDescriptor(new BLE2902());
  chForce->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);
  adv->start();

  Serial.println("[BLE] Advertising as 'Aquiles x1'");
}

void loop() {
  tphase += 0.02f;

  if (deviceConnected && streamEnabled) {
    uint32_t now = millis();

    if (now - lastIMUms >= IMU_PERIOD_MS) {
      lastIMUms = now;
      float imuL[6]; genIMU(imuL, 0.0f);
      float imuR[6]; genIMU(imuR, 1.1f);
      notifyFloatArray(chImuL, imuL, 6);
      notifyFloatArray(chImuR, imuR, 6);
    }

    if (now - lastENCms >= ENC_PERIOD_MS) {
      lastENCms = now;
      float encL = genAngle(0.2f);
      float encR = genAngle(1.3f);
      notifyOneFloat(chEncL, encL);
      notifyOneFloat(chEncR, encR);
    }

    if (now - lastFORCEms >= FORCE_PERIOD_MS) {
      lastFORCEms = now;
      float f3[3]; genForce(f3);
      notifyFloatArray(chForce, f3, 3);
    }
  }

  delay(1);
}