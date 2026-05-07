#include "LoRaAPI.h"

#include <SPI.h>
#include "PinDefs.h"

#define LORA_POLL_PERIOD_MS      5

#define LORA_FREQ_MHZ           915.0
#define LORA_BW_KHZ             125.0
#define LORA_SF                 9
#define LORA_CR                 7
#define LORA_TX_POWER_DBM       14
#define LORA_PREAMBLE_LEN       8
#define LORA_TCXO_VOLTAGE       1.6

static SPIClass loraSPI(FSPI);

static LR1121 radio = new Module(
    LORA_CS,
    RADIOLIB_NC,     // DIO9/IRQ not routed on PCB
    LORA_RST,
    LORA_BUSY,
    loraSPI
);

enum class LoRaApiOp {
    Idle,
    Tx,
    Rx
};

static LoRaApiOp currentOp = LoRaApiOp::Idle;

static uint32_t txStartUs = 0;
static RadioLibTime_t txTimeoutUs = 0;

static uint32_t rxStartMs = 0;
static uint32_t rxTimeoutMs = 0;

static constexpr uint32_t LORA_IRQ_MASK_ALL =
    RADIOLIB_LR11X0_IRQ_TX_DONE |
    RADIOLIB_LR11X0_IRQ_RX_DONE |
    RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED |
    RADIOLIB_LR11X0_IRQ_SYNC_WORD_HEADER_VALID |
    RADIOLIB_LR11X0_IRQ_HEADER_ERR |
    RADIOLIB_LR11X0_IRQ_CRC_ERR |
    RADIOLIB_LR11X0_IRQ_CAD_DONE |
    RADIOLIB_LR11X0_IRQ_CAD_DETECTED |
    RADIOLIB_LR11X0_IRQ_TIMEOUT;

static void LoRaApiPrintIrqFlags(uint32_t irq)
{
    // Serial.print("  IRQ flags: 0x");
    Serial.println(irq, HEX);

    if (irq & RADIOLIB_LR11X0_IRQ_TX_DONE) {
        // Serial.println("    TX_DONE");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_RX_DONE) {
        // Serial.println("    RX_DONE");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED) {
        // Serial.println("    PREAMBLE_DETECTED");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_SYNC_WORD_HEADER_VALID) {
        // Serial.println("    SYNC_WORD_HEADER_VALID");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_HEADER_ERR) {
        // Serial.println("    HEADER_ERR");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_CRC_ERR) {
        // Serial.println("    CRC_ERR");
    }

    if (irq & RADIOLIB_LR11X0_IRQ_TIMEOUT) {
        // Serial.println("    TIMEOUT");
    }
}

static int16_t LoRaApiConfigureRfSwitch()
{
    static const uint32_t rfswitch_dio_pins[] = {
        RADIOLIB_LR11X0_DIO5,
        RADIOLIB_LR11X0_DIO6,
        RADIOLIB_LR11X0_DIO7,
        RADIOLIB_NC,
        RADIOLIB_NC
    };

    static const Module::RfSwitchMode_t rfswitch_table[] = {
        { LR11x0::MODE_STBY,  { LOW,  LOW,  LOW,  LOW, LOW } },
        { LR11x0::MODE_RX,    { LOW,  LOW,  HIGH, LOW, LOW } },
        { LR11x0::MODE_TX,    { LOW,  HIGH, LOW,  LOW, LOW } },
        { LR11x0::MODE_TX_HP, { HIGH, LOW,  LOW,  LOW, LOW } },

        // Not used for 915 MHz LoRa.
        { LR11x0::MODE_TX_HF, { LOW,  LOW,  LOW,  LOW, LOW } },
        { LR11x0::MODE_GNSS,  { LOW,  LOW,  LOW,  LOW, LOW } },
        { LR11x0::MODE_WIFI,  { LOW,  LOW,  LOW,  LOW, LOW } },

        END_OF_MODE_TABLE,
    };

    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);

    return RADIOLIB_ERR_NONE;
}

int16_t LoRaApiInit(bool txRole)
{
    currentOp = LoRaApiOp::Idle;

    loraSPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, LORA_CS);

    int16_t state = radio.begin(
        LORA_FREQ_MHZ,
        LORA_BW_KHZ,
        LORA_SF,
        LORA_CR,
        RADIOLIB_LR11X0_LORA_SYNC_WORD_PRIVATE,
        LORA_TX_POWER_DBM,
        LORA_PREAMBLE_LEN,
        LORA_TCXO_VOLTAGE
    );

    // Serial.print("[LR1121] begin state = ");
    // Serial.println(state);

    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = LoRaApiConfigureRfSwitch();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = radio.clearIrqFlags(LORA_IRQ_MASK_ALL);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    return RADIOLIB_ERR_NONE;
}

bool LoRaApiIsBusy()
{
    return currentOp != LoRaApiOp::Idle;
}

int16_t LoRaApiStartTransmit(const char* payload)
{
    if (LoRaApiIsBusy()) {
        return LORA_API_BUSY;
    }

    if (payload == nullptr) {
        return RADIOLIB_ERR_NONE;
    }

    const size_t len = strlen(payload);

    if (len == 0) {
        return RADIOLIB_ERR_NONE;
    }

    if (len > RADIOLIB_LR11X0_MAX_PACKET_LENGTH) {
        return RADIOLIB_ERR_PACKET_TOO_LONG;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(payload);

    int16_t state = radio.standby();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = radio.clearIrqFlags(LORA_IRQ_MASK_ALL);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = radio.startTransmit(data, len);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    txStartUs = micros();

    txTimeoutUs = radio.getTimeOnAir(len);
    txTimeoutUs = (txTimeoutUs * 3) / 2;
    txTimeoutUs += 500000UL;

    currentOp = LoRaApiOp::Tx;

    return RADIOLIB_ERR_NONE;
}

int16_t LoRaApiPollTransmit()
{
    if (currentOp != LoRaApiOp::Tx) {
        return RADIOLIB_ERR_NONE;
    }

    uint32_t irq = radio.getIrqFlags();

    if (irq & RADIOLIB_LR11X0_IRQ_TX_DONE) {
        int16_t state = radio.finishTransmit();
        currentOp = LoRaApiOp::Idle;
        return state;
    }

    if (irq & RADIOLIB_LR11X0_IRQ_TIMEOUT) {
        // Serial.println("[LoRaAPI][TX] radio IRQ timeout");
        LoRaApiPrintIrqFlags(irq);

        radio.finishTransmit();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_TX_TIMEOUT;
    }

    if ((uint32_t)(micros() - txStartUs) > txTimeoutUs) {
        // Serial.println("[LoRaAPI][TX] software TX timeout");
        LoRaApiPrintIrqFlags(irq);

        radio.finishTransmit();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_TX_TIMEOUT;
    }

    return LORA_API_BUSY;
}

int16_t LoRaApiStartReceive(uint32_t timeoutMs)
{
    if (LoRaApiIsBusy()) {
        return LORA_API_BUSY;
    }

    int16_t state = radio.standby();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = radio.clearIrqFlags(LORA_IRQ_MASK_ALL);
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    state = radio.startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        return state;
    }

    rxStartMs = millis();
    rxTimeoutMs = timeoutMs;

    currentOp = LoRaApiOp::Rx;

    return RADIOLIB_ERR_NONE;
}

int16_t LoRaApiPollReceive(String& received)
{
    received = "";

    if (!LoRaApiIsBusy()) {
        return RADIOLIB_ERR_NONE;
    }

    uint32_t irq = radio.getIrqFlags();

    if (irq & RADIOLIB_LR11X0_IRQ_RX_DONE) {
        uint8_t buffer[RADIOLIB_LR11X0_MAX_PACKET_LENGTH + 1] = { 0 };

        size_t len = radio.getPacketLength();

        if (len > RADIOLIB_LR11X0_MAX_PACKET_LENGTH) {
            len = RADIOLIB_LR11X0_MAX_PACKET_LENGTH;
        }

        int16_t state = radio.readData(buffer, len);
        int16_t finishState = radio.finishReceive();

        currentOp = LoRaApiOp::Idle;

        if (state == RADIOLIB_ERR_NONE) {
            buffer[len] = '\0';
            received = String(reinterpret_cast<char*>(buffer));
            return RADIOLIB_ERR_NONE;
        }

        if (finishState != RADIOLIB_ERR_NONE) {
            // Serial.print("[LoRaAPI][RX] finishReceive failed: ");
            // Serial.println(finishState);
        }

        return state;
    }

    if (irq & RADIOLIB_LR11X0_IRQ_CRC_ERR) {
        // Serial.println("[LoRaAPI][RX] CRC error IRQ");
        LoRaApiPrintIrqFlags(irq);

        radio.finishReceive();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_CRC_MISMATCH;
    }

    if (irq & RADIOLIB_LR11X0_IRQ_HEADER_ERR) {
        // Serial.println("[LoRaAPI][RX] header error IRQ");
        LoRaApiPrintIrqFlags(irq);

        radio.finishReceive();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_CRC_MISMATCH;
    }

    if (irq & RADIOLIB_LR11X0_IRQ_TIMEOUT) {
        // Serial.println("[LoRaAPI][RX] radio RX timeout IRQ");
        LoRaApiPrintIrqFlags(irq);

        radio.finishReceive();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_RX_TIMEOUT;
    }

    if ((uint32_t)(millis() - rxStartMs) > rxTimeoutMs) {
        radio.finishReceive();
        currentOp = LoRaApiOp::Idle;

        return RADIOLIB_ERR_RX_TIMEOUT;
    }

    return LORA_API_BUSY;
}

float LoRaApiGetRSSI()
{
    return radio.getRSSI();
}

float LoRaApiGetSNR()
{
    return radio.getSNR();
}