#include "Outputs.h"

void OutputsTask(void* pvParameters) 
{
	Serial.println("[Output Task] Started");
	QueueHandle_t dynoQueue = *((OutputsTaskParameters*)pvParameters)->dynoQueue;
	DynoData data;

	while (1) 
	{
		if(xQueueReceive(dynoQueue, &data, portMAX_DELAY) == pdPASS)
		{
			// Serial.printf("%.2f,%.2f,%d\n", data.torque, data.horsepower, data.rpm);
			Serial.printf("%.2f,%.2f,%d\n", 30.0, 100.0, data.rpm);
		}
	}
}
