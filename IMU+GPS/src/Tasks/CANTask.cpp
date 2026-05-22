#include <CANTask.h>
#include <DataAcqTask.h>
#include <Arduino.h>
#include <PinDefs.h>
#include <string.h>
#include "driver/twai.h"

#define CAN_ID_ACCEL 0x600
#define CAN_ID_GYRO 0x601
#define CAN_ID_LAT_LNG 0x602
#define CAN_ID_SPEED_COURSE 0x603

#define CAN_TX_TIMEOUT_MS 10
#define CAN_TASK_PERIOD_MS 50

#define IMU_SCALE_FACTOR 1.0f
#define LAT_LNG_SCALE_FACTOR 1000000.0f // When decoding: gps = gps / 1000000.0f 
#define SPEED_COURSE_SCALE_FACTOR 10.0f // When decoding: gps = gps / 10.0f

#define FRAME_LEN 8

/* static void packint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 4 2-byte (int16_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: offset_3 - The fourth offset value.
 * @param: scale_factor - The scale factor for the data.
 */
static void packint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor) {
    const int16_t data_0 = (int16_t)(offset_0 * scale_factor);
    const int16_t data_1 = (int16_t)(offset_1 * scale_factor);
    const int16_t data_2 = (int16_t)(offset_2 * scale_factor);
    const int16_t data_3 = (int16_t)(offset_3 * scale_factor);

    buf[0] = (uint8_t)(data_0 & 0xFF);
    buf[1] = (uint8_t)((data_0 >> 8) & 0xFF);
    buf[2] = (uint8_t)(data_1 & 0xFF);
    buf[3] = (uint8_t)((data_1 >> 8) & 0xFF);
    buf[4] = (uint8_t)(data_2 & 0xFF);
    buf[5] = (uint8_t)((data_2 >> 8) & 0xFF);
    buf[6] = (uint8_t)(data_3 & 0xFF);
    buf[7] = (uint8_t)((data_3 >> 8) & 0xFF);
}

/* static void packuint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 4 2-byte (uint16_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: offset_3 - The fourth offset value.
 * @param: scale_factor - The scale factor for the data.
 */
static void packuint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor) {
    const uint16_t data_0 = (uint16_t)(offset_0 * scale_factor);
    const uint16_t data_1 = (uint16_t)(offset_1 * scale_factor);
    const uint16_t data_2 = (uint16_t)(offset_2 * scale_factor);
    const uint16_t data_3 = (uint16_t)(offset_3 * scale_factor);

    buf[0] = (uint8_t)(data_0 & 0xFF);
    buf[1] = (uint8_t)((data_0 >> 8) & 0xFF);
    buf[2] = (uint8_t)(data_1 & 0xFF);
    buf[3] = (uint8_t)((data_1 >> 8) & 0xFF);
    buf[4] = (uint8_t)(data_2 & 0xFF);
    buf[5] = (uint8_t)((data_2 >> 8) & 0xFF);
    buf[6] = (uint8_t)(data_3 & 0xFF);
    buf[7] = (uint8_t)((data_3 >> 8) & 0xFF);
}


/* static void packint32Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 2 4-byte (int32_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: scale_factor - The scale factor for the data.
 */
static void packint32Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float scale_factor) {
    const int32_t data_0 = (int32_t)(offset_0 * scale_factor);
    const int32_t data_1 = (int32_t)(offset_1 * scale_factor);

    buf[0] = (uint8_t)((data_0 >> 0) & 0xFF);
    buf[1] = (uint8_t)((data_0 >> 8) & 0xFF);
    buf[2] = (uint8_t)((data_0 >> 16) & 0xFF);
    buf[3] = (uint8_t)((data_0 >> 24) & 0xFF);
    buf[4] = (uint8_t)((data_1 >> 0) & 0xFF);
    buf[5] = (uint8_t)((data_1 >> 8) & 0xFF);
    buf[6] = (uint8_t)((data_1 >> 16) & 0xFF);
    buf[7] = (uint8_t)((data_1 >> 24) & 0xFF);
}

/* static esp_err_t transmitint16Frame(uint32_t can_id, float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor)
 * @brief: Transmits a CAN frame in int16_t format.
 * @param: can_id - The CAN ID for the message.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: offset_3 - The fourth offset value.
 * @param: scale_factor - The scale factor for the data.
 * @return: ESP_OK if transmission was successful, otherwise an error code.
 */
static esp_err_t transmitint16Frame(uint32_t can_id, float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor, bool isSigned = true) {
    uint8_t data[FRAME_LEN];

    if(isSigned) {
        packint16Frame(data, offset_0, offset_1, offset_2, offset_3, scale_factor);
    } else {
        packuint16Frame(data, offset_0, offset_1, offset_2, offset_3, scale_factor);
    }

    const twai_message_t msg = {
        .flags = 0,
        .identifier = can_id,
        .data_length_code = FRAME_LEN,
        .data = {data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]}
    };

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));
    if(err != ESP_OK) {
        Serial.println("[CAN] Failed to transmit TWAI message");
    }
    
    return err;
}

/* static esp_err_t transmitint32Frame(uint32_t can_id, float offset_0, float offset_1, float scale_factor)
 * @brief: Transmits a CAN frame in int32_t format.
 * @param: can_id - The CAN ID for the message.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: scale_factor - The scale factor for the data.
 * @return: ESP_OK if transmission was successful, otherwise an error code.
 */
static esp_err_t transmitint32Frame(uint32_t can_id, float offset_0, float offset_1, float scale_factor) {
    uint8_t data[FRAME_LEN];
    packint32Frame(data, offset_0, offset_1, scale_factor);

    const twai_message_t msg = {
        .flags = 0,
        .identifier = can_id,
        .data_length_code = FRAME_LEN,
        .data = {data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]}
    };

    esp_err_t err = twai_transmit(&msg, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS));
    if(err != ESP_OK) {
        Serial.println("[CAN] Failed to transmit TWAI message");
    }
    
    return err;
}

static void initCAN() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if(err != ESP_OK) {
        Serial.println("[CAN] Failed to install TWAI driver");
    }

    err = twai_start();
    if(err != ESP_OK) {
        Serial.println("[CAN] Failed to start TWAI driver");
    }

    Serial.println("[CAN] TWAI driver installed and started");
}

void CANTask(void* pvParameters) {
    CANTaskParameters* params = (CANTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);

    Serial.println("[CAN] Task Started");

    initCAN();
    IMUGPSData_t packet;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(CAN_TASK_PERIOD_MS);

    for (;;) {
        if (xQueueReceive(data_queue, &packet, pdMS_TO_TICKS(CAN_TX_TIMEOUT_MS)) != pdTRUE) {
            continue;
        }

        if(!packet.imu_updated || !packet.gps_updated) {
            Serial.println("[CAN] WARNING: Stale data");
            continue;
        }

        transmitint16Frame(CAN_ID_ACCEL, packet.accel_x, packet.accel_y, packet.accel_z, 0, IMU_SCALE_FACTOR);
        transmitint16Frame(CAN_ID_GYRO, packet.gyro_x, packet.gyro_y, packet.gyro_z, 0, IMU_SCALE_FACTOR);
        transmitint32Frame(CAN_ID_LAT_LNG, packet.latitude, packet.longitude, LAT_LNG_SCALE_FACTOR);
        transmitint16Frame(CAN_ID_SPEED_COURSE, packet.speed, packet.course, 0, 0, SPEED_COURSE_SCALE_FACTOR, false);
        // TODO: Transmit data to CAN Bus here
    }
}
