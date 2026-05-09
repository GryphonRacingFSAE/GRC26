#ifndef UI_BATTERY_VOLTAGE_H
#define UI_BATTERY_VOLTAGE_H
#include "DataAcqTask.h"

void ui_battery_voltage_init();
void ui_battery_voltage_update(const EcuData_t* data);

#endif // UI_BATTERY_VOLTAGE_H