#include "assert.h"
#include "CAN.h"
#include "PinDefs.h"
#include "driver/twai.h"

// Setup CAN in NORMAL mode so it can ACK
static twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();

static twai_filter_config_t f_config;

static void initCAN()
{
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    // RPM = 0x520 first 2 bytes
    // 11 bit ID, only reading first 2 bytes
    f_config.acceptance_code = (0x520 << 21);
    f_config.acceptance_mask = ~(0x7FF << 21);
    f_config.single_filter = true;

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK)
    {
        Serial.printf("[CAN] Driver install failed: %d\n", err);
        return;
    }

    err = twai_start();
    if (err != ESP_OK)
    {
        Serial.printf("[CAN] Start failed: %d\n", err);
        return;
    }
}

void CANTask(void *pvParameters)
{
    Serial.println("[CAN Task] Started");
    initCAN();
    twai_message_t msg;

    QueueHandle_t dynoQueue = *((CANTaskParameters*)pvParameters)->dynoQueue;

    for (;;)
    {
      printf("Waiting for CAN messages...\n");
      esp_err_t err = twai_receive(&msg, portMAX_DELAY);

      if (err != ESP_OK)
      {
        continue;
      }

      if (msg.data_length_code >= 2)
      {
        uint16_t rpm = (int16_t)(msg.data[0] | (msg.data[1] << 8));
        printf("RPM: %d\n", rpm);

        DynoData data;
        data.rpm        = rpm;
        data.torque     = 0;
        data.horsepower = 0;

        xQueueSend(dynoQueue, &data, 0);
      }
    }
}