#include <Arduino.h>
#include <TaskFactory.h>

void setup()
{
    Serial.begin(115200);
    delay(300);
    createTasks();
}

void loop()
{
    vTaskDelete(nullptr);
}
