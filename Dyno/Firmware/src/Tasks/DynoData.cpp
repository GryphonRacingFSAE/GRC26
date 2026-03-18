#include "DynoData.h"

#include "HX711.h"
#include "PinDefs.h"

HX711 scale;

float calibration_factor = -100;

void DynoDataTask(void* pvParameters) {
	Serial.println("[Data Task] Started");

	scale.begin(STRAIN_AMP_DATA, STRAIN_AMP_CLK);
	scale.set_scale();
	scale.tare();
	long zero_factor = scale.read_average(); // Get a baseline reading
	Serial.print("Zero factor: ");
	Serial.println(zero_factor);
	
	DynoDataTaskParameters* params = (DynoDataTaskParameters*)pvParameters;
	QueueHandle_t inputQueue       = *(params->inputQueue);	
	QueueHandle_t outputQueue      = *(params->outputQueue);

	DynoData data;

	while (1) 
    {
		delay(100);
		if(xQueueReceive(inputQueue, &data, portMAX_DELAY) == pdPASS)
		{
			/// TODO: Read torque from Strain Gauge 
			data.torque = 5;
			data.horsepower = (data.rpm * data.torque) / 5252;
		}

		scale.set_scale(calibration_factor); 

		Serial.print("Reading: ");
		Serial.print(scale.get_units(), 1);
		Serial.print(" lbs"); 
		Serial.print(" calibration_factor: ");
		Serial.print(calibration_factor);
		Serial.println();

		if(Serial.available())
		{
			char temp = Serial.read();
			if(temp == '+' || temp == 'a')
			calibration_factor += 10;
			else if(temp == '-' || temp == 'z')
			calibration_factor -= 10;
		}

		xQueueSend(outputQueue, &data, 0);
	}
}