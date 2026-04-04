#include "DynoData.h"

#include "HX711.h"
#include "PinDefs.h"

HX711 scale;
float calibration_factor = -70000.0;

void DynoDataTask(void* pvParameters) {
	Serial.println("[Data Task] Started");

	scale.begin(STRAIN_AMP_DATA, STRAIN_AMP_CLK);
	scale.set_scale();
	// scale.tare();
	long zero_factor = scale.read_average(); // Get a baseline reading
	Serial.print("Zero factor: ");
	Serial.println(zero_factor);
	
	DynoDataTaskParameters* params = (DynoDataTaskParameters*)pvParameters;
	QueueHandle_t inputQueue       = *(params->inputQueue);	
	QueueHandle_t outputQueue      = *(params->outputQueue);

	DynoData data;

	scale.set_scale(calibration_factor); 

	while (1) 
    {
		if(xQueueReceive(inputQueue, &data, portMAX_DELAY) == pdPASS)
		{
			data.torque = scale.get_units();
			data.horsepower = (data.rpm * data.torque) / 5252.0;
		}

		// if(Serial.available())
		// {
		// 	char temp = Serial.read();
		// 	if(temp == '+' || temp == 'a')
		// 	calibration_factor += 10;
		// 	else if(temp == '-' || temp == 'z')
		// 	calibration_factor -= 10;
		// }

		xQueueSend(outputQueue, &data, 0);
	}
}