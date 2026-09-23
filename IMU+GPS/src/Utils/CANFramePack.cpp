#include "CANFramePack.h"

void packint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor) {
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

void packuint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor) {
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

void packint32Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float scale_factor) {
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