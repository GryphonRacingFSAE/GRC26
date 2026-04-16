#include <Arduino.h>
#include <Wire.h>
#include <TaskFactory.h>
#include <PinDefs.h>

void setup() { 
  Serial.begin(115200);
  createTasks();
}

void loop() {
  vTaskDelete(NULL); // kill arduino loop, using RTOS scheduler 
}
