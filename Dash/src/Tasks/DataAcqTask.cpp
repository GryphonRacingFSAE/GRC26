#include <Arduino.h>
#include <DataAcqTask.h>

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
        case 0x520: {  // RPM + Throttle

          uint16_t raw_rpm = (rx_msg.data[1] << 8) | rx_msg.data[0];

          uint16_t raw_throttle = (rx_msg.data[3] << 8) | rx_msg.data[2];

          if (xSemaphoreTake(params->guiMutex, portMAX_DELAY) == pdTRUE) {
            params->sharedData->rpm = raw_rpm;
            params->sharedData->throttlePercent =
                raw_throttle * 0.1f;  // adjust scale if needed

            xSemaphoreGive(params->guiMutex);
          }

          break;
        }

        case 0x522: {  // Vehicle Speed

          uint16_t raw_speed = (rx_msg.data[1] << 8) | rx_msg.data[0];

          if (xSemaphoreTake(params->guiMutex, portMAX_DELAY) == pdTRUE) {
            params->sharedData->vehicleSpeedKph =
                raw_speed * 0.01f;  // adjust scale

            xSemaphoreGive(params->guiMutex);
          }

          break;
        }

        case 0x523: {  // Driven Wheel Speed

          uint16_t raw_wheel_speed = (rx_msg.data[1] << 8) | rx_msg.data[0];

          if (xSemaphoreTake(params->guiMutex, portMAX_DELAY) == pdTRUE) {
            params->sharedData->wheelSpeedDrivenKph = raw_wheel_speed * 0.01f;

            xSemaphoreGive(params->guiMutex);
          }

          break;
        }

        case 0x530: {  // Coolant Temp

          int16_t raw_coolant = (rx_msg.data[1] << 8) | rx_msg.data[0];

          if (xSemaphoreTake(params->guiMutex, portMAX_DELAY) == pdTRUE) {
            ecuData.coolantTempC = raw_coolant * 0.1f;

            xSemaphoreGive(params->guiMutex);
          }

          break;
        }

        case 0x536: {  // Oil Pressure (bool) + Oil Temp

          bool oil_pressure_flag = (rx_msg.data[0] & 0x01);

          int16_t raw_temp = (rx_msg.data[3] << 8) | rx_msg.data[2];

          if (xSemaphoreTake(params->guiMutex, portMAX_DELAY) == pdTRUE) {
            params->sharedData->oilPressureKpa = oil_pressure_flag;
            params->sharedData->oilTempC = raw_temp * 0.1f;

            xSemaphoreGive(params->guiMutex);
          }

          break;
        }
      }
    }
  }
}
