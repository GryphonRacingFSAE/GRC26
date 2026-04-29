#include <CANTask.h>
#include <DataAcqTask.h>
#include <Arduino.h>
#include <PinDefs.h>
#include <string.h>
#include "driver/twai.h"

#define CAN_ID_IMU 0x600
#define CAN_ID_GPS 0x601

#define CAN_TX_TIMEOUT_MS 10
#define CAN_TASK_PERIOD_MS 50

#define IMU_SCALE_FACTOR 1.0f 
#define GPS_SCALE_FACTOR 10.0f

#define FRAME_LEN 8

/* static void packBytes(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float scale_factor)
 * @brief: Packs data into a byte array for CAN transmission. Byte ordering: Little Endian
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: scale_factor - The scale factor for the data.
 */
static void packBytes(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float scale_factor) {
    const int16_t data_0 = (int16_t)(offset_0 * scale_factor);
    const int16_t data_1 = (int16_t)(offset_1 * scale_factor);
    const int16_t data_2 = (int16_t)(offset_2 * scale_factor);

    buf[0] = (uint8_t)(data_0 & 0xFF);
    buf[1] = (uint8_t)((data_0 >> 8) & 0xFF);
    buf[2] = (uint8_t)(data_1 & 0xFF);
    buf[3] = (uint8_t)((data_1 >> 8) & 0xFF);
    buf[4] = (uint8_t)(data_2 & 0xFF);
    buf[5] = (uint8_t)((data_2 >> 8) & 0xFF);
    buf[6] = 0x00; 
    buf[7] = 0x00; 
}

/* static esp_err_t transmitFrame(uint32_t can_id, float offset_0, float offset_1, float offset_2, float scale_factor)
 * @brief: Transmits a CAN frame with the specified data.
 * @param: can_id - The CAN ID for the message.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: scale_factor - The scale factor for the data.
 * @return: ESP_OK if transmission was successful, otherwise an error code.
 */
static esp_err_t transmitFrame(uint32_t can_id, float offset_0, float offset_1, float offset_2, float scale_factor) {
    uint8_t data[FRAME_LEN];
    packBytes(data, offset_0, offset_1, offset_2, scale_factor);

    const twai_message_t msg = {
        .flags = 0,
        .identifier = can_id,
        .data_length_code = FRAME_LEN,
        .data = {data[0], data[1], data[2], data[3], data[4], data[5], 0x00, 0x00}
    };

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));
    if (err != ESP_OK) {
        Serial.printf("[CAN] ERROR: Failed to transmit TWAI message (err=%d)\n", err);
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

        transmitFrame(CAN_ID_IMU, imuData.accel_x, imuData.accel_y, imuData.accel_z, IMU_SCALE_FACTOR);
        Serial.println("CAN Task");
        // TODO: Transmit data to CAN Bus here
        // xQueueSend(*params->dataQueue, &myPacket, 0);
    }
}
