#ifndef CAN_FRAME_PACK_H
#define CAN_FRAME_PACK_H

#include <stdint.h>

#define FRAME_LEN 8

/* void packint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 4 2-byte (int16_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: offset_3 - The fourth offset value.
 * @param: scale_factor - The scale factor for the data.
 */
void packint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor);

/* void packuint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 4 2-byte (uint16_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: offset_2 - The third offset value.
 * @param: offset_3 - The fourth offset value.
 * @param: scale_factor - The scale factor for the data.
 */
void packuint16Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float offset_2, float offset_3, float scale_factor);

/* void packint32Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float scale_factor)
 * @brief: Split CAN frame 8-byte data payload into 2 4-byte (int32_t) values, applying the specified scale factor to each value. The resulting bytes are stored in the provided buffer.
 * @param: buf - The buffer to store the packed data.
 * @param: offset_0 - The first offset value.
 * @param: offset_1 - The second offset value.
 * @param: scale_factor - The scale factor for the data.
 */
void packint32Frame(uint8_t buf[FRAME_LEN], float offset_0, float offset_1, float scale_factor);

#endif // CAN_FRAME_PACK_H