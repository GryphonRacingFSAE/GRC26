#ifndef GuiTask_H
#define GuiTask_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include "esp_heap_caps.h"

typedef struct {
    SemaphoreHandle_t* guiMutex;
    QueueHandle_t*     dataQueue;
} GuiTaskParameters;

/**Shared Variables
 * 1. Updated from DataAcqTask
 * 2. Declared in GuiTask.h as volatile extern for CAN to update values in timer callbacks, and for GuiTask to read and update the UI.
*/

extern volatile uint16_t rpm_value;
extern volatile uint8_t speed_value;  
extern volatile uint8_t throttle_value;
extern volatile int16_t coolantTemp_value;
extern volatile int16_t oilTemp_value;

void GuiTask(void* pvParameters);

#endif // GuiTask_H