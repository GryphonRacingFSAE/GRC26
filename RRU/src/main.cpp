#include <Arduino.h>
#include <TaskFactory.h>


void setup() 
{
  Serial.begin(921600);
  createTasks();
}

void loop() 
{
  vTaskDelete(NULL);
}