#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const int PIN_SCK  = 36;
static const int PIN_MISO = 37;
static const int PIN_MOSI = 35;
static const int PIN_CS   = 39;

///Stacks
#define tskINIT_STACK					(configMINIMAL_STACK_SIZE)
#define tskBLINK_STACK					(configMINIMAL_STACK_SIZE)
#define tskSPI_TX_STACK				(configMINIMAL_STACK_SIZE)
///Priorities
#define tskINIT_PRIORITY			    (tskIDLE_PRIORITY + 3UL)
#define tskBLINK_PRIORITY               (tskIDLE_PRIORITY + 1UL)
#define tskSPI_TX_PRIORITY              (tskIDLE_PRIORITY + 2UL)

void InitTask(void *params);
void BlinkTask(void *params);
void vSpiTxTask(void* *params);
