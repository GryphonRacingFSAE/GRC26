#include <Arduino.h>
#include <Wire.h>
#include <TaskFactory.h>
#include <PinDefs.h>

void setup() {
    Serial.begin(115200);
    delay(2000);
    createTasks();
}

void loop() {
    vTaskDelete(NULL); // Kill Arduino Loop, run on RTOS scheduler
}
