#include <DataAcqTask.h>
#include <PinDefs.h>
#include <Arduino.h>

#include "driver/twai.h"

#define DATA_TASK_PERIOD_MS 20

// CAN Init
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)LCD_CAN_TX, (gpio_num_t)LCD_CAN_RX, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS(); 
static twai_filter_config_t f_config;

static void initCAN() {
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);

    // if (err == ESP_OK) {
    //     Serial.println("[DataAcq] TWAI Driver Installed");
    // }

    err = twai_start();
    // if (err == ESP_OK) {
    //     Serial.println("[DataAcq] TWAI Started");
    // }
}

static void DecodeCanData(const twai_message_t* msg, EcuData_t* dataOut) {
    if(msg->data_length_code >= 2) {
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
        case 0x530: // Coolant Temp + Battery Voltage
            {
                uint16_t batteryVoltage_raw = (msg->data[1] << 8) | msg->data[0];
                uint16_t clt_raw = (msg->data[7] << 8) | msg->data[6];
                dataOut->batteryVoltage = batteryVoltage_raw * 0.01f;
                dataOut->clt = clt_raw * 0.1f;
                break;
            }    
        case 0x536: // Oil Pressure
            {
                uint16_t oilPressure_raw = (msg->data[5] << 8) | msg->data[4];
                dataOut->oilPressure = oilPressure_raw * 0.1f;
                break;
            }
        case 0x525: // Brake Pressure
            {
                uint16_t bp_raw = (msg->data[1] << 8) | msg->data[0];
                dataOut->bp = bp_raw;
                break;
            }
        }
    }

}

void DataAcqTask(void* pvParameters) {
    DataAcqTaskParameters* params = (DataAcqTaskParameters*)pvParameters;
    QueueHandle_t data_queue = *(params->dataQueue);
    initCAN();
    twai_message_t rx_msg;

    Serial.println("[Data] Task Started");

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(DATA_TASK_PERIOD_MS);
    EcuData_t data = {0};

    for (;;) {
        esp_err_t err = twai_receive(&rx_msg, portMAX_DELAY);   

        if(err != ESP_OK) {
            continue;
        }

        DecodeCanData(&rx_msg, &data);
        xQueueSend(data_queue, &data, 0); 
    }
}
