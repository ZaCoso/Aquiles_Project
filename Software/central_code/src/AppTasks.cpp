#include "AppTasks.h"

namespace Aquiles {

    // ================= BLE =================
    namespace BLE {
        NimBLECharacteristic* pCharacteristic = nullptr;
        volatile bool deviceConnected = false;
        std::string lastValue = "0";

        void MyCallbacks::onWrite(NimBLECharacteristic* pChar) {
            std::string value = std::string(pChar->getValue().c_str());
            if (!value.empty()) {
                Serial.print("Recibido via BLE: ");
                Serial.println(value.c_str());
                lastValue = value;
            }
        }

        void MyServerCallbacks::onConnect(NimBLEServer* /*pServer*/) {
            deviceConnected = true;
            Serial.println("Dispositivo conectado");
        }

        void MyServerCallbacks::onDisconnect(NimBLEServer* /*pServer*/) {
            deviceConnected = false;
            Serial.println("Dispositivo desconectado");
        }

        void init() {
            NimBLEDevice::init(DEVICE_NAME);

            NimBLEServer* pServer = NimBLEDevice::createServer();
            pServer->setCallbacks(new MyServerCallbacks());

            NimBLEService* pService = pServer->createService(SERVICE_UUID);

            pCharacteristic = pService->createCharacteristic(
                CHARACTERISTIC_UUID,
                NIMBLE_PROPERTY::READ |
                NIMBLE_PROPERTY::WRITE |
                NIMBLE_PROPERTY::NOTIFY
            );

            pCharacteristic->setValue("0");
            pCharacteristic->setCallbacks(new MyCallbacks());
            pService->start();

            NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(SERVICE_UUID);
            pAdvertising->setScanResponse(true);
            pAdvertising->start();

            Serial.println("ESP32 BLE listo para conexiones");

            xTaskCreatePinnedToCore(
                TaskNotify,
                "BLE_NOTIFY",
                4096,
                nullptr,
                1,
                nullptr,
                1
            );
        }

        void enviarEstado() {
            static int bat = 0;
            bat = (bat + 1) % 101;
            String mensaje = "BAT:" + String(bat);

            if (pCharacteristic) {
                pCharacteristic->setValue(mensaje.c_str());
                pCharacteristic->notify();
            }

            Serial.print("Enviado: ");
            Serial.println(mensaje);
        }

        void TaskNotify(void* /*pv*/) {
            const TickType_t period = pdMS_TO_TICKS(3000);
            for (;;) {
                if (deviceConnected) {
                    enviarEstado();
                    Serial.println(lastValue.c_str());
                }
                vTaskDelay(period);
            }
        }
    }

    // ================= TAREAS BÁSICAS =================
    namespace Tasks {
        void Init(void* /*params*/) {
            pinMode(46, OUTPUT);
            vTaskDelete(nullptr);
        }

        void Blink(void* /*params*/) {
            for (;;) {
                digitalWrite(46, !digitalRead(46));
                Serial.printf("Rk.Test1\r\n");
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }

    // ================= SPI BUS =================
    namespace SPIBus {

        static SPISettings RP2040_SETTINGS(1000000, MSBFIRST, SPI_MODE0);
        static SPISettings DEV_B_SETTINGS(8000000, MSBFIRST, SPI_MODE3);

        static SemaphoreHandle_t spiMutex = nullptr;

        static inline void lock()   { xSemaphoreTake(spiMutex, portMAX_DELAY); }
        static inline void unlock() { xSemaphoreGive(spiMutex); }
        static inline uint8_t xfer(uint8_t b) { return SPI.transfer(b); }

        uint8_t crc8_sum(const uint8_t* data, size_t len, uint8_t init) {
            uint8_t crc = init;
            for (size_t i = 0; i < len; ++i) {
                crc ^= data[i];
                for (uint8_t k = 0; k < 8; ++k)
                    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
            }
            return crc;
        }

        void init() {
            pinMode(CS_RP2040, OUTPUT); digitalWrite(CS_RP2040, HIGH);
            pinMode(CS_DEV_B,  OUTPUT); digitalWrite(CS_DEV_B,  HIGH);
            SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, CS_RP2040);
            if (!spiMutex) spiMutex = xSemaphoreCreateMutex();
            Serial.println("SPI Bus listo (un bus, múltiples CS).");
        }

        bool rp2040_send_packet(int16_t v1, int16_t v2, int16_t v3,
                                uint8_t resp6[6], uint32_t timeout_ms)
        {
            uint8_t frame[9] = {
                0xAA, 6,
                (uint8_t)(v1 & 0xFF), (uint8_t)((v1 >> 8) & 0xFF),
                (uint8_t)(v2 & 0xFF), (uint8_t)((v2 >> 8) & 0xFF),
                (uint8_t)(v3 & 0xFF), (uint8_t)((v3 >> 8) & 0xFF),
                0
            };
            frame[8] = crc8_sum(&frame[2], 6);

            lock();
            SPI.beginTransaction(RP2040_SETTINGS);
            digitalWrite(CS_RP2040, LOW);

            for (auto b : frame) (void)xfer(b);

            uint32_t t0 = millis();
            size_t got = 0;
            while (got < 6) {
                resp6[got++] = xfer(0x00);
                if ((millis() - t0) > timeout_ms) {
                    digitalWrite(CS_RP2040, HIGH);
                    SPI.endTransaction();
                    unlock();
                    return false;
                }
            }

            digitalWrite(CS_RP2040, HIGH);
            SPI.endTransaction();
            unlock();
            return true;
        }

        bool devB_transfer(const uint8_t* tx, uint8_t* rx, size_t n) {
            if (!n) return true;
            lock();
            SPI.beginTransaction(DEV_B_SETTINGS);
            digitalWrite(CS_DEV_B, LOW);

            for (size_t i = 0; i < n; ++i) {
                uint8_t t = tx ? tx[i] : 0x00;
                uint8_t r = xfer(t);
                if (rx) rx[i] = r;
            }

            digitalWrite(CS_DEV_B, HIGH);
            SPI.endTransaction();
            unlock();
            return true;
        }

        void parse_triplet(const uint8_t* p, int16_t& a, int16_t& b, int16_t& c) {
            a = (int16_t)(p[0] | (p[1] << 8));
            b = (int16_t)(p[2] | (p[3] << 8));
            c = (int16_t)(p[4] | (p[5] << 8));
        }

        void vSpiTask_RP2040(void* /*pv*/) {
            int16_t v1 = 100, v2 = -200, v3 = 300;
            uint8_t rx6[6];
            for (;;) {
                bool ok = rp2040_send_packet(v1, v2, v3, rx6, 20);
                if (ok) {
                    int16_t r1, r2, r3;
                    parse_triplet(rx6, r1, r2, r3);
                    Serial.printf("[RP2040] RESP: %d, %d, %d\r\n", r1, r2, r3);
                } else {
                    Serial.println("[RP2040] Timeout/FAIL");
                }
                v1 += 1; v2 -= 2; v3 += 3;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        }

        void vSpiTask_DevB(void* /*pv*/) {
            uint8_t tx[3] = {0x9F, 0x00, 0x00};
            uint8_t rx[3];
            for (;;) {
                bool ok = devB_transfer(tx, rx, sizeof(tx));
                if (ok) {
                    Serial.printf("[DEV_B] RX: %02X %02X %02X\r\n", rx[0], rx[1], rx[2]);
                } else {
                    Serial.println("[DEV_B] FAIL");
                }
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    } // namespace SPIBus

} // namespace Aquiles