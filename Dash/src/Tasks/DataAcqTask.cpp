#include <DataAcqTask.h>
#include <Arduino.h>

#include "driver/twai.h"

#define DATA_TASK_PERIOD_MS 20

static void DecodeCanData(const twai_message_t* msg, EcuData_t* dataOut) {
    // TODO: Optimize the code.
    // Currently using default CAN ID from MaxxECU Race
    switch(msg->identifier) {
        case 0x520: // RPM + TPS
            {
                uint16_t rpm_raw = (msg->data[1] << 8) | msg->data[0];
                uint16_t tps_raw = (msg->data[3] << 8) | msg->data[2];
                dataOut->rpm = rpm_raw;
                dataOut->tps = tps_raw * 0.1f;
                break;
            }
        case 0x522: // Vehicle Speed
            {
                uint16_t vss_raw = (msg->data[7] << 8) | msg->data[6];
                dataOut->speed = vss_raw * 0.1f;
                break;
            }
        case 0x523: // WheelSpeedAvgDriven
            {
                uint16_t wss_raw = (msg->data[3] << 8) | msg->data[2];
                dataOut->wheelSpeed = wss_raw * 0.1f;
                break;
            }
        case 0x530: // Coolant Temp
            {
                uint16_t clt_raw = (msg->data[7] << 8) | msg->data[6];
                dataOut->clt = clt_raw * 0.1f;
                break;
            }    
        case 0x536: // Oil Pressure
            {
                uint16_t oilPressure_raw = (msg->data[5] << 8) | msg->data[4];
                dataOut->oilPressure = oilPressure_raw * 0.1f;
                break;
            }
        }
}

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);
    twai_message_t rx_msg;

    Serial.println("[Data] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);
    EcuData_t data = {0};

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        esp_err_t err = twai_receive(&rx_msg, portMAX_DELAY);   

        // blocking when no message received so no serial flooding
        if(err == ESP_OK) {
            // Debugging code 
            Serial.println("Data Acquisition Task");  
            Serial.printf("[DataAcq] CAN ID: 0x%03X\tDLC: %d", rx_msg.identifier, rx_msg.data_length_code);
            for(int i = 0; i < rx_msg.data_length_code; i++) {
                Serial.printf("\t0x%02X", rx_msg.data[i]);
            }
            DecodeCanData(&rx_msg, &data);
            xQueueSend(data_queue, &data, 0);
        }
        // TODO: Read CAN Bus / Sensors here
        // xQueueSend(*params->dataQueue, &myPacket, 0);

    }
}
