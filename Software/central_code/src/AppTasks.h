#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <NimBLEDevice.h>

namespace Aquiles {

    // ====== Config BLE ======
    constexpr const char* DEVICE_NAME       = "Aquiles X1";
    constexpr const char* SERVICE_UUID      = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    constexpr const char* CHARACTERISTIC_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

    // ====== Pines SPI ======
    constexpr int PIN_SCK   = 18;
    constexpr int PIN_MISO  = 19;
    constexpr int PIN_MOSI  = 23;

    // ====== Chip Selects ======
    constexpr int CS_RP2040 = 5;
    constexpr int CS_DEV_B  = 15;

    // ====== Stack & Priority ======
    constexpr uint16_t tskINIT_STACK      = configMINIMAL_STACK_SIZE;
    constexpr uint16_t tskBLINK_STACK     = 4 * configMINIMAL_STACK_SIZE;
    constexpr uint16_t tskSPI_TX_STACK    = configMINIMAL_STACK_SIZE;

    constexpr UBaseType_t tskINIT_PRIORITY   = tskIDLE_PRIORITY + 3UL;
    constexpr UBaseType_t tskBLINK_PRIORITY  = tskIDLE_PRIORITY + 1UL;
    constexpr UBaseType_t tskSPI_TX_PRIORITY = tskIDLE_PRIORITY + 2UL;

    // ====== Forward namespaces ======
    namespace BLE {
        extern NimBLECharacteristic* pCharacteristic;
        extern volatile bool deviceConnected;
        extern std::string lastValue;

        class MyCallbacks : public NimBLECharacteristicCallbacks {
            void onWrite(NimBLECharacteristic* pCharacteristic) override;
        };

        class MyServerCallbacks : public NimBLEServerCallbacks {
            void onConnect(NimBLEServer* pServer) override;
            void onDisconnect(NimBLEServer* pServer) override;
        };

        void init();
        void enviarEstado();
        void TaskNotify(void* pv);
    }

    namespace SPIBus {
        void init();
        bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                                uint8_t resp6[6], uint32_t timeout_ms = 20);
        bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n);
        uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init = 0x00);
        void parse_triplet(const uint8_t* p, int16_t& a, int16_t& b, int16_t& c);

        void vSpiTask_RP2040(void* pv);
        void vSpiTask_DevB(void* pv);
    }

    namespace Tasks {
        void Init(void* params);
        void Blink(void* params);
    }

} // namespace Aquiles