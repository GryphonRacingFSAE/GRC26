#include <DataAcqTask.h>
#include <Arduino.h>
#include "driver/twai.h"

#define DATA_TASK_PERIOD_MS 20

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;

    Serial.println("[Data] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("Data Acquisition Task");
        twai_message_t rx_msg;

        // Non-blocking receive (since task runs every 20ms)
        if (twai_receive(&rx_msg, 0) == ESP_OK) {

            switch (rx_msg.identifier) {

            case 0x0A2: {   // Motor Controller Temps

                int16_t hot_spot_temp =
                    (int16_t)((rx_msg.data[3] << 8) | rx_msg.data[2]);

                int16_t coolant_temp =
                    (int16_t)((rx_msg.data[1] << 8) | rx_msg.data[0]);

                if (xSemaphoreTake(params->dataMutex, portMAX_DELAY) == pdTRUE) {
                    params->sharedData->motor_controller_temp = hot_spot_temp;
                    params->sharedData->coolant_temp = coolant_temp;
                    xSemaphoreGive(params->dataMutex);
                }

                break;
            }

            case 0x0A7: {   // DC Bus Voltage

                int16_t dc_bus_voltage =
                    (int16_t)((rx_msg.data[1] << 8) | rx_msg.data[0]);

                if (xSemaphoreTake(params->dataMutex, portMAX_DELAY) == pdTRUE) {
                    params->sharedData->tractive_voltage = dc_bus_voltage;
                    xSemaphoreGive(params->dataMutex);
                }

                break;
            }

            case 0x0B0: {   // Motor Speed

                int16_t motor_speed =
                    (int16_t)((rx_msg.data[3] << 8) | rx_msg.data[2]);

                if (xSemaphoreTake(params->dataMutex, portMAX_DELAY) == pdTRUE) {
                    params->sharedData->motor_speed = motor_speed;
                    xSemaphoreGive(params->dataMutex);
                }

                break;
            }

            case 0x0E0: {   // BMS Max Temp

                int8_t max_temp = (int8_t)rx_msg.data[0];

                if (xSemaphoreTake(params->dataMutex, portMAX_DELAY) == pdTRUE) {
                    params->sharedData->bms_max_temp = max_temp;
                    xSemaphoreGive(params->dataMutex);
                }

                break;
            }

            default:
                break;
            }
        }
    }
}
