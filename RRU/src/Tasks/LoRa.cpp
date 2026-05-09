#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include "LoRa.h"
#include "AssertMsg.h"
#include "PinDefs.h"   

#define LORA_TASK_PERIOD_MS 100

static SPIClass loraSPI(FSPI);

static LR1121 radio = new Module(
    LORA_CS,          
    RADIOLIB_NC,      // IRQ not connected on LoRa
    LORA_RST,         
    LORA_BUSY,        
    loraSPI
);

void LoRaInit() 
{
    loraSPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, LORA_CS);

    Serial.print("[LR1121] Initializing");
    int16_t state = radio.begin();

    /// @warning If LoRa module fails to initialize, MCU will hard fault and print an error message.
    ASSERT_MSG(state == RADIOLIB_ERR_NONE, "[LR1121] Failed to initialize: " + String(state));
}

void LoRaTask(void* pvParameters)
{
    LoRaTaskParameters* params = (LoRaTaskParameters*)pvParameters;

    Serial.println("[LoRaTask] Task started");

    LoRaInit();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(LORA_TASK_PERIOD_MS);

    for(;;) 
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Serial.println("[LoRaTask] Running");
    }
}
