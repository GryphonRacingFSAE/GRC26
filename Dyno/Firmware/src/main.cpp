#include <Arduino.h>

// 1D Lookup Table (LUT) modeling a 600cc engine torque curve
const int MAP_POINTS = 10;
const int rpm_map[MAP_POINTS] =    {1500, 3000, 4500, 6000, 8000, 10000, 11500, 13000, 14500, 15000};
const int torque_map[MAP_POINTS] = {20, 28, 34, 38, 42, 46,  48,  45,  38,  30};

int current_rpm = 1500;
int throttle_position = 1; 

// Linear Interpolation (Standard ECU mapping technique)
int GetTorqueFromMap(int rpm) {
  if (rpm <= rpm_map[0]) return torque_map[0];
  if (rpm >= rpm_map[MAP_POINTS - 1]) return torque_map[MAP_POINTS - 1];

  for (int i = 0; i < MAP_POINTS - 1; i++) {
    if (rpm >= rpm_map[i] && rpm < rpm_map[i+1]) {
      float slope = (torque_map[i+1] - torque_map[i]) / (rpm_map[i+1] - rpm_map[i]);
      return torque_map[i] + slope * (rpm - rpm_map[i]);
    }
  }
  return 0;
}

int CalculateHorsePower(float torque, float rpm) {
  return (torque * rpm) / 5252;
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  int base_torque = GetTorqueFromMap(current_rpm);
  
  int actual_torque = base_torque * throttle_position;
  
  int load_torque = (current_rpm * current_rpm) * 0.00000025; 
  load_torque += 5.0; 
  
  int net_torque = actual_torque - load_torque;
  
  int rpm_change = net_torque * 15.0 * 0.02; // dt = 20ms
  current_rpm += rpm_change;
  
  // RPM Hard limits
  if (current_rpm < 1500.0) current_rpm = 1500.0; 
  if (current_rpm > 15000.0) current_rpm -= 500;  
  
  int hp = CalculateHorsePower(actual_torque, current_rpm);

  Serial.printf("%d,%d,%d\n", actual_torque, hp, current_rpm);
  
  delay(20); // 50Hz sample rate
}