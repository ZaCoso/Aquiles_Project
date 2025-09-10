#include "AppTasks.h"

///Semaphore
xSemaphoreHandle semph_LeftMotor;
xSemaphoreHandle semph_RightMotor;

/// Queues

void InitTask(void *params){
    semph_LeftMotor = xSemaphoreCreateBinary();
    semph_RightMotor = xSemaphoreCreateBinary();
    pinMode(46, OUTPUT);
    vTaskDelete(NULL);
}
void BlinkTask(void *params){
    while(1){
    digitalWrite(46, !digitalRead(46));
    vTaskDelay(pdMS_TO_TICKS(500));}
}
