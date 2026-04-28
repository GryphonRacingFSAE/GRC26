#include <CANTask.h>
#include <DataAcqTask.h>
#include <Arduino.h>
#include <PinDefs.h>
#include <string.h>
#include "driver/twai.h"

#define CAN_ID_ACCEL 0x100
#define CAN_ID_GYRO 0x101

#define ACCEL_SCALE_FACTOR 1.0f 
#define GYRO_SCALE_FACTOR 10.0f

#define CAN_TX_TIMEOUT_MS 10
#define CAN_TASK_PERIOD_MS 50

static esp_err_t transmitIMUFrame(uint32_t id, float offset_0, float offset_1, float offset_2, float scale_factor) {
    twai_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.identifier = id;
    msg.extd = 0; // Standard CAN frame
    msg.rtr = 0; // Data frame
    msg.data_length_code = 8; 

    int16_t data_0 = (int16_t)(offset_0 * scale_factor);
    int16_t data_1 = (int16_t)(offset_1 * scale_factor);
    int16_t data_2 = (int16_t)(offset_2 * scale_factor);

    memcpy(&msg.data[0], &data_0, sizeof(int16_t));
    memcpy(&msg.data[2], &data_1, sizeof(int16_t));
    memcpy(&msg.data[4], &data_2, sizeof(int16_t));

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));

    if(err != ESP_OK) {
        Serial.printf("[CAN] ERROR: Failed to transmit CAN frame (err=%d)\n", err); 
    }
    
    return err;
}

static void initCAN() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        Serial.printf("[CAN] ERROR: Failed to install TWAI driver (err=%d)\n", err);
        return;
    }

    err = twai_start();
    if (err != ESP_OK) {
        Serial.printf("[CAN] ERROR: Failed to start TWAI driver (err=%d)\n", err);
        return;
    }

    Serial.println("[CAN] TWAI driver installed and started");
}

void CANTask(void* pvParameters) {
    CANTaskParameters* params = (CANTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);

    Serial.println("[CAN] Task Started");

    initCAN();
    IMUData_t imuData;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(CAN_TASK_PERIOD_MS);

    for (;;) {
        if(xQueueReceive(data_queue, &imuData, xFrequency) != pdTRUE) {
            continue;
        }

        if(!imuData.updated) {
            Serial.println("[CAN] WARNING: Received IMU data that is not updated");
            continue;
        }

        transmitIMUFrame(CAN_ID_ACCEL, imuData.accel_x, imuData.accel_y, imuData.accel_z, ACCEL_SCALE_FACTOR);
        transmitIMUFrame(CAN_ID_GYRO, imuData.gyro_x, imuData.gyro_y, imuData.gyro_z, GYRO_SCALE_FACTOR);
        
        Serial.println("CAN Task");
        // TODO: Transmit data to CAN Bus here
        // xQueueSend(*params->dataQueue, &myPacket, 0);
    }
}
