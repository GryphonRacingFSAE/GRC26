#include "Outputs.h"

void OutputsTask(void* pvParameters) {
	Serial.println("[Output Task] Started");
	QueueHandle_t dynoQueue = *((OutputsTaskParameters*)pvParameters)->dynoQueue;
	DynoData data;

	while (1) 
	{
		if(xQueueReceive(dynoQueue, &data, portMAX_DELAY) == pdPASS)
		{
			Serial.printf("%d,%d,%d\n", data.torque, data.horsepower, data.rpm);
		}
	}
}
