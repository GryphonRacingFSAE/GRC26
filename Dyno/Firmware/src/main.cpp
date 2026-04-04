#include <Arduino.h>
#include <TaskFactory.h>

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("[Setup] Initializing...");
  createTasks();
}

void loop() {
}