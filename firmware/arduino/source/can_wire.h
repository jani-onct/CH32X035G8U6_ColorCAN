#ifndef COLORCAN_CAN_WIRE_H
#define COLORCAN_CAN_WIRE_H

#include <stdbool.h>
#include <stdint.h>

#define CAN_WIRE_MAX_BITS 135

struct can_wire_frame {
    uint8_t bits[CAN_WIRE_MAX_BITS];
    uint8_t length;
    uint8_t ack_index;
};

bool can_encode_base(struct can_wire_frame *frame, uint16_t id,
                     const uint8_t *data, uint8_t dlc);

#endif
