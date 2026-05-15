#include <Arduino.h>
#include <PinDefs.h>
#include "driver/twai.h"

#include "CANTask.h"

// -----------------------------
// TWAI / CAN setup
// -----------------------------
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX,
    (gpio_num_t)CAN_RX,
    TWAI_MODE_NORMAL
);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Conservative safety placeholders. Tune these to your car.
static constexpr uint16_t OIL_PRESSURE_LOW_KPA_X10       = 1000; // 100.0 kPa
static constexpr uint16_t OIL_PRESSURE_CHECK_MIN_RPM      = 1500;
static constexpr uint16_t FUEL_PRESSURE_LOW_KPA_X10      = 2500; // 250.0 kPa
static constexpr int16_t  COOLANT_TEMP_HIGH_C_X10        = 1050; // 105.0 C
static constexpr uint16_t BATTERY_LOW_V_X100             = 1150; // 11.50 V
static constexpr int16_t  LAMBDA_ERROR_HIGH_X1000        = 80;   // 0.080 lambda
static constexpr uint16_t LAMBDA_ERROR_CHECK_MIN_TPS_X10 = 300;  // 30.0 %

struct EcuTelemetryState
{
    bool hasCanData;

    // 0x520
    uint16_t rpm;
    uint16_t tps_x10;
    uint16_t map_kpa_x10;
    uint16_t lambda_avg_x1000;

    // 0x522
    uint16_t vehicle_speed_kph_x10;

    // 0x526
    uint16_t status_bits;

    // 0x527
    int16_t lambda_target_x1000;

    // 0x530
    uint16_t battery_v_x100;
    uint16_t baro_kpa_x10;
    int16_t intake_air_temp_c_x10;
    int16_t coolant_temp_c_x10;

    // 0x534
    uint16_t egt_delta_c;
    uint16_t ecu_temp_c;
    uint16_t ecu_error_count;
    uint16_t ecu_lost_sync_count;

    // 0x537
    uint16_t fuel_pressure_kpa_x10;
    uint16_t coolant_pressure_kpa_x10;

    // 0x538
    uint16_t brake_pressure_kpa_x10;

    // 0x600
    int16_t acceleration_x_mg;
    int16_t acceleration_y_mg;
    int16_t acceleration_z_mg;

    uint32_t lastCanRxMs;
    uint16_t lambda_error_x1000;
};

static EcuTelemetryState ecu = {};
static uint16_t telemetrySeq = 0;

static uint16_t lastStatusBits = 0;
static uint16_t lastAlertFlags = 0;
static uint16_t lastEcuErrorCount = 0;
static uint16_t lastLostSyncCount = 0;
static uint16_t lastKnockCount = 0;

static uint32_t lastFastTxMs = 0;
static uint32_t lastSlowTxMs = 0;

/// @brief Converts a little-endian unsigned 16-bit value from a byte array at the specified index.
/// @param data Pointer to the byte array containing the data.
/// @param byteIndex The starting index of the 16-bit value in the byte array.
/// @return The converted 16-bit value.
static inline uint16_t u16_le(const uint8_t* data, uint8_t byteIndex)
{
    return (uint16_t)data[byteIndex] | ((uint16_t)data[byteIndex + 1] << 8);
}


/// @brief Converts a little-endian signed 16-bit value from a byte array at the specified index.
/// @param data Pointer to the byte array containing the data.
/// @param byteIndex The starting index of the 16-bit value in the byte array.
/// @return The converted 16-bit value.
static inline int16_t i16_le(const uint8_t* data, uint8_t byteIndex)
{
    return (int16_t)u16_le(data, byteIndex);
}

/// @brief Saturation function for signed 16-bit integers.
/// @param value The input value to saturate.
/// @return The saturated 16-bit value.
static inline int16_t sat_i16(int32_t value)
{
    if (value > INT16_MAX) 
    {
        return INT16_MAX;
    }
    if (value < INT16_MIN) 
    {
        return INT16_MIN;
    }
    return (int16_t)value;
}

/// @brief Absolute value function for signed 16-bit integers.
/// @param value The input value.
/// @return The absolute value as an unsigned 16-bit integer.
static inline uint16_t abs_i16(int16_t value)
{
    return (value < 0) ? (uint16_t)(-value) : (uint16_t)value;
}

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

static void updateLambdaError()
{
    const int32_t error = (int32_t)ecu.lambda_avg_x1000 - (int32_t)ecu.lambda_target_x1000;
    ecu.lambda_error_x1000 = sat_i16(error);
}

static bool decodeCanData(const twai_message_t* msg)
{
    if (msg == nullptr) {
        return false;
    }

    // Ignore extended and remote frames
    if (msg->extd || msg->rtr) {
        return false;
    }

    const uint8_t* d = msg->data;
    const uint8_t len = msg->data_length_code;

    bool decoded = true;

    switch (msg->identifier) 
    {
        case 0x520: // RPM, TPS, MAP, Lambda Average
            if (len < 8) return false;
            ecu.rpm              = u16_le(d, 0);
            ecu.tps_x10          = u16_le(d, 2);
            ecu.map_kpa_x10      = u16_le(d, 4);
            ecu.lambda_avg_x1000 = u16_le(d, 6);
            updateLambdaError();
            break;

        case 0x526: // Status bits
            if (len < 2) return false;
            ecu.status_bits = u16_le(d, 0);
            break;

        case 0x527: // Lambda target
            if (len < 8) return false;
            ecu.lambda_target_x1000 = i16_le(d, 6);
            updateLambdaError();
            break;

        case 0x530: // Battery, IAT, Coolant Temp
            if (len < 8) return false;
            ecu.battery_v_x100        = u16_le(d, 0);
            ecu.baro_kpa_x10          = u16_le(d, 2);
            ecu.intake_air_temp_c_x10 = (int16_t)u16_le(d, 4);
            ecu.coolant_temp_c_x10    = (int16_t)u16_le(d, 6);
            break;

        case 0x534: // ECU temp/errors/lost sync, EGT delta
            if (len < 8) return false;
            ecu.egt_delta_c         = u16_le(d, 0);
            ecu.ecu_temp_c          = u16_le(d, 2);
            ecu.ecu_error_count     = u16_le(d, 4);
            ecu.ecu_lost_sync_count = u16_le(d, 6);
            break;

        case 0x537: // Fuel pressure, coolant pressure
            if (len < 8) return false;
            ecu.fuel_pressure_kpa_x10      = u16_le(d, 0);
            ecu.coolant_pressure_kpa_x10   = u16_le(d, 4);
            break;

        case 0x538: // Brake Pressure
            ecu.brake_pressure_kpa_x10 = u16_le(d, 0);
            break;

        case 0x600: // Acceleration (IMU)
            if (len < 8) return false;
            ecu.acceleration_x_mg = i16_le(d, 0);
            ecu.acceleration_y_mg = i16_le(d, 2);
            ecu.acceleration_z_mg = i16_le(d, 4);
            break;

        default:
            decoded = false;
            break;
    }

    if (decoded) {
        ecu.hasCanData = true;
        ecu.lastCanRxMs = millis();
    }

    return decoded;
} 

static bool sendTelemetryPacket(QueueHandle_t queue, const TelemetryPacket& packet)
{
    if (queue == nullptr) {
        return false;
    }

    // Non-blocking. If LoRa is still busy and the queue fills, drop newest.
    return xQueueSend(queue, &packet, 0) == pdTRUE;
}

static uint16_t buildAlertFlags()
{
    uint16_t flags = ALERT_NONE;

    if (ecu.status_bits != lastStatusBits) 
    {
        flags |= ALERT_STATUS_CHANGED;
    }

    if (ecu.ecu_error_count != lastEcuErrorCount) 
    {
        flags |= ALERT_ECU_ERROR_CHANGED;
    }

    if (ecu.ecu_lost_sync_count != lastLostSyncCount) 
    {
        flags |= ALERT_LOST_SYNC_CHANGED;
    }

    return flags;
}

static void sendFastPacket(QueueHandle_t queue)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_FAST;

    TelemetryFastPacket& p = packet.data.fast;
    p.ms                    = millis();
    p.seq                   = telemetrySeq++;
    p.rpm                   = ecu.rpm;
    p.tps_x10               = ecu.tps_x10;
    p.map_kpa_x10           = ecu.map_kpa_x10;
    p.lambda_avg_x1000      = ecu.lambda_avg_x1000;
    p.lambda_error_x1000    = ecu.lambda_error_x1000;
    p.fuel_pressure_kpa_x10 = ecu.fuel_pressure_kpa_x10;
    p.coolant_temp_c_x10    = ecu.coolant_temp_c_x10;
    p.battery_v_x100        = ecu.battery_v_x100;
    p.vehicle_speed_kph_x10 = ecu.vehicle_speed_kph_x10;
    p.status_bits           = ecu.status_bits;

    sendTelemetryPacket(queue, packet);
}

static void sendSlowPacket(QueueHandle_t queue)
{
    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_SLOW;

    TelemetrySlowPacket& p = packet.data.slow;
    p.ms                         = millis();
    p.seq                        = telemetrySeq++;
    p.intake_air_temp_c_x10      = ecu.intake_air_temp_c_x10;
    p.ecu_error_count            = ecu.ecu_error_count;
    p.ecu_lost_sync_count        = ecu.ecu_lost_sync_count;
    p.ecu_temp_c                 = ecu.ecu_temp_c;
    p.egt_delta_c                = ecu.egt_delta_c;
    p.coolant_pressure_kpa_x10   = ecu.coolant_pressure_kpa_x10;

    sendTelemetryPacket(queue, packet);
}

static void sendEventPacketIfNeeded(QueueHandle_t queue)
{
    const uint16_t alertFlags = buildAlertFlags();

    const bool shouldSend =
        (alertFlags != lastAlertFlags) ||
        (ecu.status_bits != lastStatusBits) ||
        (ecu.ecu_error_count != lastEcuErrorCount) ||
        (ecu.ecu_lost_sync_count != lastLostSyncCount);

    if (!shouldSend) 
    {
        return;
    }

    TelemetryPacket packet = {};
    packet.type = TELEMETRY_PACKET_EVENT;

    TelemetryEventPacket& p = packet.data.event;
    p.ms                    = millis();
    p.seq                   = telemetrySeq++;
    p.alert_flags           = alertFlags;
    p.status_bits           = ecu.status_bits;
    p.rpm                   = ecu.rpm;
    p.fuel_pressure_kpa_x10 = ecu.fuel_pressure_kpa_x10;
    p.coolant_temp_c_x10    = ecu.coolant_temp_c_x10;
    p.battery_v_x100        = ecu.battery_v_x100;
    p.ecu_error_count       = ecu.ecu_error_count;
    p.ecu_lost_sync_count   = ecu.ecu_lost_sync_count;

    sendTelemetryPacket(queue, packet);

    lastAlertFlags = alertFlags;
    lastStatusBits = ecu.status_bits;
    lastEcuErrorCount = ecu.ecu_error_count;
    lastLostSyncCount = ecu.ecu_lost_sync_count;
}

static void sendPeriodicPacketsIfDue(QueueHandle_t queue)
{
    if (!ecu.hasCanData) {
        return;
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - lastFastTxMs) >= TELEMETRY_FAST_PERIOD_MS) {
        sendFastPacket(queue);
        lastFastTxMs = now;
    }

    if ((uint32_t)(now - lastSlowTxMs) >= TELEMETRY_SLOW_PERIOD_MS) {
        sendSlowPacket(queue);
        lastSlowTxMs = now;
    }
}

void CANTask(void* pvParameters)
{
    CANTaskParameters* params = reinterpret_cast<CANTaskParameters*>(pvParameters);

    configASSERT(params != nullptr);
    configASSERT(params->dataQueue != nullptr);

    QueueHandle_t telemetryQueue = params->dataQueue;

    Serial.println("[CAN] Task started");

    if (!initCAN()) 
    {
        Serial.println("[CAN] Init failed; deleting task");
        vTaskDelete(nullptr);
        return;
    }

    twai_message_t rxMsg = {};

    for (;;) 
    {
        const esp_err_t err = twai_receive(&rxMsg, portMAX_DELAY);

        if (err != ESP_OK) {
            // Do not spin aggressively if TWAI returns an error.
            vTaskDelay(pdMS_TO_TICKS(CAN_TASK_PERIOD_MS));
            continue;
        }

        const bool decoded = decodeCanData(&rxMsg);

        if (decoded) {
            sendEventPacketIfNeeded(telemetryQueue);
            sendPeriodicPacketsIfDue(telemetryQueue);
        }
    }
}
