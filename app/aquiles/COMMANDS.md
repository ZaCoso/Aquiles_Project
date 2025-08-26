# 📡 Protocolo de Comunicación BLE - Exoesqueleto

Este documento define el protocolo de comandos utilizado para establecer comunicación entre una aplicación móvil (iOS/Android) y un exoesqueleto controlado mediante un ESP32 utilizando Bluetooth Low Energy (BLE). El objetivo principal es facilitar tanto la telemetría como el ajuste en tiempo real del nivel de asistencia.

---

## 📦 Formato General del Comando

Todos los comandos BLE enviados desde la app deben cumplir el siguiente formato:

COMANDO:VALOR

- **Separador**: `:` (dos puntos)
- **COMANDO**: Código corto en mayúsculas que identifica la acción
- **VALOR**: Puede ser un número, una palabra clave o el signo `?` para consultas

---

## 📘 Comandos Disponibles

| Comando | Descripción                 | Valor esperado           | Ejemplo     | Notas                                         |
| ------- | --------------------------- | ------------------------ | ----------- | --------------------------------------------- |
| `AID`   | Nivel de asistencia del exo | Entero entre 0 y 100 (%) | `AID:80`    | Ajusta el porcentaje de ayuda muscular        |
| `ON`    | Encender el sistema         | `1`                      | `ON:1`      | Activa los motores y sensores principales     |
| `OFF`   | Apagar el sistema           | `1`                      | `OFF:1`     | Detiene todos los sistemas del exoesqueleto   |
| `CAL`   | Iniciar calibración         | `1`                      | `CAL:1`     | Ejecuta la rutina de calibración inicial      |
| `SPD`   | Velocidad de asistencia     | Entero entre 0 y 100     | `SPD:30`    | Afecta la respuesta dinámica en ciertos modos |
| `MODE`  | Modo operativo              | `AUTO`, `MANU`, `DEMO`   | `MODE:AUTO` | Cambia el modo de funcionamiento              |
| `BAT`   | Consulta de batería         | `?`                      | `BAT:?`     | Devuelve el porcentaje de batería restante    |
| `STA`   | Estado general del sistema  | `?`                      | `STA:?`     | Devuelve información sobre el estado actual   |

---

## 📥 Respuestas del ESP32

El ESP32 responde con el mismo formato de texto plano:

| Tipo  | Ejemplo                | Descripción                                 |
| ----- | ---------------------- | ------------------------------------------- |
| `OK`  | `OK:AID SET TO 80`     | Comando recibido y ejecutado correctamente  |
| `ERR` | `ERR:VAL OUT OF RANGE` | Error al interpretar o aplicar el comando   |
| `BAT` | `BAT:76`               | Porcentaje actual de batería                |
| `STA` | `STA:OK` o `STA:ERR1`  | Estado general o código de error específico |

---

## 🧪 Ejemplo de Flujo

```plaintext
APP → ESP32:    ON:1
ESP32 → APP:    OK:SYSTEM ON

APP → ESP32:    AID:65
ESP32 → APP:    OK:AID SET TO 65

APP → ESP32:    BAT:?
ESP32 → APP:    BAT:82
```
