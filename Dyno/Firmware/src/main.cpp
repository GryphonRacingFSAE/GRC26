#include <Arduino.h>

static bool isAccelerating = true;
static int current_torque = 0;
static int current_rpm = 2000;

int TorqueSimulation(int torque) {
  if (isAccelerating && torque < 50) 
  { 
    return torque + 2; 
  } else 
  {
    isAccelerating = false;
    if (torque > 0) return torque - 2;
    return 0;
  }
}

int RpmSimulation(int rpm) 
{
  if (isAccelerating && rpm < 11000) 
  { 
    return rpm + 100; 
  } else 
  {
    isAccelerating = false;
    if (rpm > 2000) return rpm - 100;
    
    isAccelerating = true; 
    return 2000; 
  }
}

float CalculateHorsePower(int torque, int rpm) 
{
  return ((float)torque * (float)rpm) / 5252.0;
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  current_torque = TorqueSimulation(current_torque);
  current_rpm = RpmSimulation(current_rpm);
  float horsepower = CalculateHorsePower(current_torque, current_rpm);

  Serial.printf("%d,%d,%.2f\n", current_rpm, current_torque, horsepower, current_rpm);

  // 50Hz sample rate 
  delay(20);
}