#include <Arduino.h>
#include "Outputs.h"
#include "LoRaOutputs.h"

static const char* EventTypeToCsvString(LoRaOutputEventType type)
{
    switch (type) {
        case LoRaOutputEventType::RxPacket:
            return "rx_packet";

        case LoRaOutputEventType::RxTimeout:
            return "rx_timeout";

        case LoRaOutputEventType::RxCrcMismatch:
            return "rx_crc_mismatch";

        case LoRaOutputEventType::RxError:
            return "rx_error";

        case LoRaOutputEventType::TxStarted:
            return "tx_started";

        case LoRaOutputEventType::TxDone:
            return "tx_done";

        case LoRaOutputEventType::TxError:
            return "tx_error";

        default:
            return "unknown";
    }
}

static void PrintCsvHeader()
{
    Serial.println(
        "event,"
        "rx_timestamp_ms,"
        "pkt_type,"
        "seq,"
        "tx_ms,"
        "rpm,"
        "apps,"
        "brake,"
        "temp_c,"
        "vbat,"
        "state,"
        "rssi_dbm,"
        "snr_db,"
        "rx_count,"
        "tx_count,"
        "error_code"
    );
}

static void PrintLoRaEventCsv(const LoRaOutputEvent& event)
{
    char line[512];

    if (event.type == LoRaOutputEventType::RxPacket) 
    {
        snprintf(
            line,
            sizeof(line),
            "%s,%lu,%s,%.2f,%.2f,%lu,%lu,%d\n",
            EventTypeToCsvString(event.type),
            static_cast<unsigned long>(event.timestampMs),
            event.payload,
            event.rssi,
            event.snr,
            static_cast<unsigned long>(event.rxPacketCount),
            static_cast<unsigned long>(event.txPacketCount),
            event.errorCode
        );
    } else 
    {
        snprintf(
            line,
            sizeof(line),
            "%s,%lu,,,,,,,,,,%.2f,%.2f,%lu,%lu,%d\n",
            EventTypeToCsvString(event.type),
            static_cast<unsigned long>(event.timestampMs),
            event.rssi,
            event.snr,
            static_cast<unsigned long>(event.rxPacketCount),
            static_cast<unsigned long>(event.txPacketCount),
            event.errorCode
        );
    }

    Serial.print(line);
}

void OutputsTask(void* pvParameters)
{
    OutputsTaskParameters* params = reinterpret_cast<OutputsTaskParameters*>(pvParameters);

    configASSERT(params != nullptr);
    configASSERT(params->dataQueue != nullptr);

    QueueHandle_t inputQueue = params->dataQueue;

    LoRaOutputEvent event = {};

    PrintCsvHeader();

    for (;;) {
        if (!LoRaOutputsReceive(inputQueue, &event, portMAX_DELAY)) 
        {
            continue;
        }

        PrintLoRaEventCsv(event);
    }
}