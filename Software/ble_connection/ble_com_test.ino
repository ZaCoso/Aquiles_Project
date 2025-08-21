#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Nombre del dispositivo BLE
#define DEVICE_NAME "Aquiles X1"
// UUIDs personalizados
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
std::string lastValue = "0";

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = std::string(pCharacteristic->getValue().c_str());

      if (value.length() > 0) {
        Serial.print("Recibido via BLE: ");
        Serial.println(value.c_str());

        // Por ejemplo, cambiar porcentaje de asistencia
        lastValue = value;
      }
    }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Dispositivo conectado");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Dispositivo desconectado");
    }
};

void setup() {
  Serial.begin(115200);

  BLEDevice::init(DEVICE_NAME);
  
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ   |
    BLECharacteristic::PROPERTY_WRITE  |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());  // Importante para que Flutter/iOS reciba notificaciones


  pCharacteristic->setValue("0");  // Valor inicial
  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
pAdvertising->addServiceUUID(SERVICE_UUID);  // <- ESTA LÍNEA ES CLAVE
pAdvertising->setScanResponse(true);
pAdvertising->start();

  Serial.println("ESP32 BLE listo para conexiones");
}

void loop() {
  // Podés usar el valor recibido:
    static int bat = 0; 
  if (deviceConnected) {
    enviarEstado();
    Serial.println(lastValue.c_str());
    delay(3000);
  }
}

void enviarEstado() {
  static int bat = 0;
  bat = (bat + 1) % 101;
  String mensaje = "BAT:" + String(bat);
  pCharacteristic->setValue(mensaje.c_str());
  pCharacteristic->notify();

  Serial.print("Enviado: ");
  Serial.println(mensaje);
}