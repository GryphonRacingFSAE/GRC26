#include <Arduino.h>
#include <PinDefs.h>
#include "driver/twai.h"

#include "CANTask.h"
#include "TelemetrySender.h"

// Receive only: retain existing bus timing/mode and never enqueue CAN transmissions.
static twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

static EcuTelemetryState ecu = {};
static TelemetryEventTracker eventTracker = {};
static uint16_t telemetrySeq = 0;
static uint32_t lastFastTxMs = 0;
static uint32_t lastSlowTxMs = 0;
static uint32_t lastPowertrainTxMs = 0;

static bool initCAN()
{
    f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.print("[CAN] twai_driver_install failed: ");
        Serial.println((int)err);
        return false;
    }

    err = twai_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.print("[CAN] twai_start failed: ");
        Serial.println((int)err);
        return false;
    }

    Serial.println("[CAN] TWAI started");
    return true;
}

static bool decodeCanData(const twai_message_t* msg)
{
    return msg != nullptr &&
           decodeEcuCanFrame(ecu, msg->identifier, msg->data, msg->data_length_code, millis(), msg->extd, msg->rtr);
}

static bool sendTelemetryPacket(QueueHandle_t queue, const TelemetryPacket& packet)
{
    if (queue == nullptr) {
        return false;
    }
    // Non-blocking. If LoRa is still busy and the queue fills, drop newest.
    return xQueueSend(queue, &packet, 0) == pdTRUE;
}

static void sendFastPacket(QueueHandle_t queue, uint32_t now)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_FAST;
    populateFastPacket(packet.data.fast_v2, ecu, now, telemetrySeq++);
    sendTelemetryPacket(queue, packet);
}

static void sendSlowPacket(QueueHandle_t queue, uint32_t now)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_SLOW;
    populateSlowPacket(packet.data.slow_v2, ecu, now, telemetrySeq++);
    sendTelemetryPacket(queue, packet);
}

static void sendPowertrainPacket(QueueHandle_t queue, uint32_t now)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_POWERTRAIN;
    populatePowertrainPacket(packet.data.powertrain_v2, ecu, now, telemetrySeq++);
    sendTelemetryPacket(queue, packet);
}

static void sendEventPacketIfNeeded(QueueHandle_t queue)
{
    const uint16_t flags = observeTelemetryEvents(ecu, eventTracker);
    if (flags == 0) {
        return;
    }
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_EVENT;
    populateEventPacket(packet.data.event_v2, ecu, millis(), telemetrySeq++, flags);
    sendTelemetryPacket(queue, packet);
}

static void sendPeriodicPacketsIfDue(QueueHandle_t queue)
{
    if (!ecu.hasCanData) {
        return;
    }
    const uint32_t now = millis();
    if ((uint32_t)(now - lastFastTxMs) >= TELEMETRY_FAST_PERIOD_MS) {
        sendFastPacket(queue, now);
        lastFastTxMs = now;
    }
    if ((uint32_t)(now - lastSlowTxMs) >= TELEMETRY_SLOW_PERIOD_MS) {
        sendSlowPacket(queue, now);
        lastSlowTxMs = now;
    }
    if ((uint32_t)(now - lastPowertrainTxMs) >= TelemetryV2::POWERTRAIN_PERIOD_MS) {
        sendPowertrainPacket(queue, now);
        lastPowertrainTxMs = now;
    }
}

void CANTask(void* pvParameters)
{
    CANTaskParameters* params = reinterpret_cast<CANTaskParameters*>(pvParameters);
    configASSERT(params != nullptr);
    configASSERT(params->dataQueue != nullptr);
    QueueHandle_t telemetryQueue = params->dataQueue;
    Serial.println("[CAN] Task started");

    if (!initCAN()) {
        Serial.println("[CAN] Init failed; deleting task");
        vTaskDelete(nullptr);
        return;
    }

    twai_message_t rxMsg = {};
    for (;;) {
        const esp_err_t err = twai_receive(&rxMsg, portMAX_DELAY);
        if (err != ESP_OK) {
            // Do not spin aggressively if TWAI returns an error.
            vTaskDelay(pdMS_TO_TICKS(CAN_TASK_PERIOD_MS));
            continue;
        }
        if (decodeCanData(&rxMsg)) {
            sendEventPacketIfNeeded(telemetryQueue);
            sendPeriodicPacketsIfDue(telemetryQueue);
        }
    }
}
