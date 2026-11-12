#ifndef COLORCAN_CAN_BENCH_H
#define COLORCAN_CAN_BENCH_H

#include <stdint.h>

enum can_tx_result {
    CAN_TX_ACKED,
    CAN_TX_NO_ACK,
    CAN_TX_BUSY,
    CAN_TX_ERROR,
    CAN_TX_RESULT_COUNT,
};

void can_bench_init(void);
enum can_tx_result can_bench_send(uint16_t id, const uint8_t data[8]);
void can_bench_print_error(void);

#endif
