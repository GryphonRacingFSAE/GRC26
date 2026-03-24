#include <DataAcqTask.h>
#include <Arduino.h>

#include "driver/twai.h"

#define DATA_TASK_PERIOD_MS 20

static void DecodeCanData(const twai_message_t* msg, EcuData_t* dataOut) {
    /**
     * TODO: 
     * 1. Try to combine all pirimitive data types into one CAN ID. Variables will be assigned through bit manipulation (offset).
     * 2. Check for message scale factor.
     **/
    switch(msg->identifier) {
        case 0x502: // RPM
            dataOut->tps = (msg->data[3] << 8) | msg->data[2];
            break;
        case 0x522: // Speed
            dataOut->speed = (msg->data[1] << 8) | msg->data[0];
            break;
        case 0x523: // Wheel Speed
            dataOut->wheelSpeed = (msg->data[1] << 8) | msg->data[0];
            break;
        case 0x530: // Coolant Temp
            dataOut->clt = (msg->data[1] << 8) | msg->data[0];
            dataOut->rpm = (msg->data[1] << 8) | msg->data[0];
            break;
        case 0x536: // Oil Temp
            dataOut->oilPressure_flag = (msg->data[0] & 0x01);
            dataOut->oilTemp = (msg->data[3] << 8) | msg->data[2];
            break;
        }
}

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;
    QueueHandle_t dataQueue = *(params->dataQueue);

    Serial.println("[Data] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);
    EcuData_t data = {0};

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        twai_message_t rx_msg;

        // blocking when no message received so no serial flooding
        if(twai_receive(&rx_msg, 0) == ESP_OK) {
            // Debugging code 
            Serial.println("Data Acquisition Task");
            Serial.printf("[DataAcq] CAN ID: 0x%03X\tDLC: %d", rx_msg.identifier, rx_msg.data_length_code);
            for(int i = 0; i < rx_msg.data_length_code; i++) {
                Serial.printf("\t0x%02X", rx_msg.data[i]);
            }
            DecodeCanData(&rx_msg, &data);
            Serial.printf("[DataAcq] RPM: %d  Speed: %d  TPS: %d  CLT: %.f  Oil: %.f\n", data.rpm, data.speed, data.tps, data.clt, data.oilTemp);
            xQueueOverwrite(dataQueue, &data);
        }
        // TODO: Read CAN Bus / Sensors here
        // xQueueSend(*params->dataQueue, &myPacket, 0);

    }
}
