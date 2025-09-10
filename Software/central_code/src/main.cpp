#include "AppTasks.h"
// put function declarations here:

void setup() {
  // put your setup code here, to run once:
  xTaskCreate(InitTask, "Init", tskINIT_STACK, NULL, tskINIT_PRIORITY, NULL);
  xTaskCreate(BlinkTask, "Blinky",tskBLINK_STACK, NULL, tskBLINK_PRIORITY, NULL);

}

void loop() {
  // put your main code here, to run repeatedly:
}
