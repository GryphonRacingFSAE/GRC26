#include <Arduino.h>
#include <TaskFactory.h>

void setup() 
{
  Serial.begin(115200);
  createTasks();
}

void loop() 
{
  vTaskDelete(NULL);
}
