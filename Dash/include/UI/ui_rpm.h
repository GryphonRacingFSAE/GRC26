#ifndef UI_RPM_H
#define UI_RPM_H
#include "DataAcqTask.h"
 
void ui_rpm_init();
void ui_rpm_update(const EcuData_t* data);

#endif // UI_RPM_H