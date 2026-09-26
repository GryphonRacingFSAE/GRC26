#include <Arduino.h>

#include "CANTask.h"
#include "TelemetrySender.h"

#if TELEMETRY_MOCK_DATA
#include "MockTelemetry.h"
#else
#include <PinDefs.h>
#include "driver/twai.h"

// Receive only: retain existing bus timing/mode and never enqueue CAN transmissions.
static twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX, (gpio_num_t)CAN_RX, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
#endif

static EcuTelemetryState ecu = {};
static TelemetryEventTracker eventTracker = {};
static uint16_t telemetrySeq = 0;
static uint32_t lastFastTxMs = 0;
static uint32_t lastSlowTxMs = 0;
static uint32_t lastSensorsTxMs = 0;

#if !TELEMETRY_MOCK_DATA
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
#endif

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
    populateFastPacket(packet.data.fast, ecu, now, telemetrySeq++);
    sendTelemetryPacket(queue, packet);
}

static void sendSlowPacket(QueueHandle_t queue, uint32_t now)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_SLOW;
    populateSlowPacket(packet.data.slow, ecu, now, telemetrySeq++);
    sendTelemetryPacket(queue, packet);
}

static void sendSensorsPacket(QueueHandle_t queue, uint32_t now)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_SENSORS;
    populateSensorsPacket(packet.data.sensors, ecu, now, telemetrySeq++);
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
    populateEventPacket(packet.data.event, ecu, millis(), telemetrySeq++, flags);
    sendTelemetryPacket(queue, packet);
}

static void sendPeriodicPacketsIfDue(QueueHandle_t queue)
{
    // An empty-mask heartbeat starts immediately; no CAN source is mandatory.
    const uint32_t now = millis();
    if ((uint32_t)(now - lastFastTxMs) >= TELEMETRY_FAST_PERIOD_MS) {
        sendFastPacket(queue, now);
        // Preserve the deadline phase while skipping missed slots, never catch up in a burst.
        lastFastTxMs += ((uint32_t)(now - lastFastTxMs) / TELEMETRY_FAST_PERIOD_MS) * TELEMETRY_FAST_PERIOD_MS;
    }
    if ((uint32_t)(now - lastSlowTxMs) >= TELEMETRY_SLOW_PERIOD_MS) {
        sendSlowPacket(queue, now);
        lastSlowTxMs += ((uint32_t)(now - lastSlowTxMs) / TELEMETRY_SLOW_PERIOD_MS) * TELEMETRY_SLOW_PERIOD_MS;
    }
    if ((uint32_t)(now - lastSensorsTxMs) >= TelemetryProtocol::SENSORS_PERIOD_MS) {
        sendSensorsPacket(queue, now);
        lastSensorsTxMs += ((uint32_t)(now - lastSensorsTxMs) / TelemetryProtocol::SENSORS_PERIOD_MS) *
                              TelemetryProtocol::SENSORS_PERIOD_MS;
    }
}

static uint32_t timeUntilPacket(uint32_t now, uint32_t lastDeadline, uint32_t period)
{
    const uint32_t elapsed = now - lastDeadline;
    return elapsed >= period ? 0 : period - elapsed;
}

static TickType_t telemetryReceiveWaitTicks()
{
    // Always bounded by the next telemetry deadline, including before the first CAN frame.
    const uint32_t now = millis();
    uint32_t waitMs = timeUntilPacket(now, lastFastTxMs, TELEMETRY_FAST_PERIOD_MS);
    const uint32_t slowWaitMs = timeUntilPacket(now, lastSlowTxMs, TELEMETRY_SLOW_PERIOD_MS);
    const uint32_t sensorsWaitMs = timeUntilPacket(now, lastSensorsTxMs, TelemetryProtocol::SENSORS_PERIOD_MS);
    if (slowWaitMs < waitMs) {
        waitMs = slowWaitMs;
    }
    if (sensorsWaitMs < waitMs) {
        waitMs = sensorsWaitMs;
    }
    if (waitMs == 0) {
        return 0;
    }
    const TickType_t waitTicks = pdMS_TO_TICKS(waitMs);
    return waitTicks > 0 ? waitTicks : 1;
}

void CANTask(void* pvParameters)
{
    CANTaskParameters* params = reinterpret_cast<CANTaskParameters*>(pvParameters);
    configASSERT(params != nullptr);
    configASSERT(params->dataQueue != nullptr);
    QueueHandle_t telemetryQueue = params->dataQueue;
    Serial.println("[CAN] Task started");

#if TELEMETRY_MOCK_DATA
    Serial.println("[CAN][MOCK] Enabled: generated telemetry -> LoRa; CAN disabled");
    for (;;) {
        updateMockTelemetry(ecu, millis());
        sendPeriodicPacketsIfDue(telemetryQueue);
        sendEventPacketIfNeeded(telemetryQueue);
        const TickType_t waitTicks = telemetryReceiveWaitTicks();
        vTaskDelay(waitTicks > 0 ? waitTicks : 1);
    }
#else
    if (!initCAN()) {
        Serial.println("[CAN] Init failed; deleting task");
        vTaskDelete(nullptr);
        return;
    }

    twai_message_t rxMsg = {};
    for (;;) {
        // Send on deadlines even when CAN is silent or unrelated/invalid frames arrive.
        // Cached values retain their per-source freshness status in every snapshot.
        sendPeriodicPacketsIfDue(telemetryQueue);
        const esp_err_t err = twai_receive(&rxMsg, telemetryReceiveWaitTicks());
        if (err == ESP_OK) {
            if (decodeCanData(&rxMsg)) {
                sendEventPacketIfNeeded(telemetryQueue);
            }
        } else if (err != ESP_ERR_TIMEOUT) {
            // A normal deadline timeout needs no extra delay; back off real driver errors.
            vTaskDelay(pdMS_TO_TICKS(CAN_TASK_PERIOD_MS));
        }
    }
#endif
}
