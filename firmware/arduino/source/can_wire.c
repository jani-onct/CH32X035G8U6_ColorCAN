#include "can_wire.h"

struct encoder {
    struct can_wire_frame *frame;
    uint16_t crc;
    uint8_t last;
    uint8_t run;
};

static void put_bit(struct encoder *enc, uint8_t bit, bool update_crc)
{
    if (update_crc) {
        bool feedback = ((enc->crc >> 14) ^ bit) & 1u;
        enc->crc = (enc->crc << 1) & 0x7fffu;
        if (feedback) enc->crc ^= 0x4599u;
    }
    struct can_wire_frame *frame = enc->frame;
    frame->bits[frame->length++] = bit;
    enc->run = enc->last == bit ? enc->run + 1 : 1;
    enc->last = bit;
    if (enc->run == 5) {
        enc->last ^= 1u;
        frame->bits[frame->length++] = enc->last;
        enc->run = 1;
    }
}

static void put_field(struct encoder *enc, uint16_t value, uint8_t width, bool update_crc)
{
    while (width != 0) put_bit(enc, (value >> --width) & 1u, update_crc);
}

bool can_encode_base(struct can_wire_frame *frame, uint16_t id,
                     const uint8_t *data, uint8_t dlc)
{
    if (id > 0x7ffu || dlc > 8u || (dlc != 0 && !data)) return false;
    frame->length = 0;
    struct encoder enc = {.frame = frame};
    put_bit(&enc, 0, true);                 // SOF
    put_field(&enc, id, 11, true);
    put_field(&enc, 0, 3, true);            // RTR, IDE, r0
    put_field(&enc, dlc, 4, true);
    for (unsigned i = 0; i < dlc; ++i) put_field(&enc, data[i], 8, true);
    put_field(&enc, enc.crc, 15, false);
    frame->bits[frame->length++] = 1;       // CRC delimiter
    frame->ack_index = frame->length;
    // ACK slot, ACK delimiter, seven EOF bits, three intermission bits.
    for (unsigned i = 0; i < 12; ++i) frame->bits[frame->length++] = 1;
    return true;
}
