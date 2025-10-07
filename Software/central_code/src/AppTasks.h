#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <NimBLEDevice.h>

// Nombre del dispositivo BLE
#define DEVICE_NAME "Aquiles X1"
// UUIDs personalizados
#define SERVICE_UUID        "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"


// ====== Pines del BUS SPI compartido ======
#define PIN_SCK    18
#define PIN_MISO   19
#define PIN_MOSI   23

// ====== Chip Select por dispositivo ======
#define CS_RP2040   5    // RP2040 slave
#define CS_DEV_B   15    // Otro dispositivo SPI (cámbialo según tu placa)


///Stacks
#define tskINIT_STACK					(configMINIMAL_STACK_SIZE)
#define tskBLINK_STACK					(4*configMINIMAL_STACK_SIZE)
#define tskSPI_TX_STACK				(configMINIMAL_STACK_SIZE)
///Priorities
#define tskINIT_PRIORITY			    (tskIDLE_PRIORITY + 3UL)
#define tskBLINK_PRIORITY               (tskIDLE_PRIORITY + 1UL)
#define tskSPI_TX_PRIORITY              (tskIDLE_PRIORITY + 2UL)
// Inicializa el BUS SPI, los CS y el mutex
void spiBusInit();

// ---- Dispositivo A: RP2040 (protocolo SOF/LEN/6 bytes/CRC) ----
// Envía v1,v2,v3 y lee 6 bytes de respuesta del RP2040.
// return: true si la transacción terminó OK y se recibieron 6 bytes en tiempo.
bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                        uint8_t resp6[6], uint32_t timeout_ms = 20);

// ---- Dispositivo B: genérico ----
// Transfiere n bytes (full-duplex). Si tx==nullptr, envía 0x00.
// rx puede ser nullptr si no te interesa leer.
bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n);

// Globals
extern NimBLECharacteristic* pCharacteristic;
extern volatile bool deviceConnected;   // <-- ahora coincide con el .cpp
extern std::string lastValue;

// Callbacks
class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) override;
};
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override;
  void onDisconnect(NimBLEServer* pServer) override;
};

// API
void initBLE();
void enviarEstado();
uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init = 0x00);
// FreeRTOS task
void TaskBleNotify(void* pv);       
// ---- Tareas de ejemplo (opcional) ----
void vSpiTask_RP2040(void* pv);
void vSpiTask_DevB(void* pv);

void InitTask(void *params);
void BlinkTask(void *params);
void vSpiTxTask(void *params);

