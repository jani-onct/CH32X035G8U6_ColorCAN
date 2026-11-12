#ifndef COLORCAN_CAN_PIOC_REGS_H
#define COLORCAN_CAN_PIOC_REGS_H

// SFR offsets are the same in the PIOC and the host's PIOC_BASE window.
#define PIOC_INDIR_PORT  0x00
#define PIOC_INDIR_ADDR  0x04
#define PIOC_PORT_DIR    0x0a
#define PIOC_PORT_IO     0x0b
#define PIOC_SYS_CFG     0x1c
#define PIOC_CTRL_RD     0x1d
#define PIOC_CTRL_WR     0x1e
#define PIOC_FRAME_DATA  0x20  // 17 bytes, MSB first
#define PIOC_ACK_INDEX   0x31
#define PIOC_BIT_INDEX   0x32

#define PIOC_TX_BIT      1     // PIOC_IO1 / PC19
#define PIOC_RX_INPUT    4     // PIOC_PORT_IO input for PIOC_IO0 / PC18
#define PIOC_CMD_PENDING 5     // PIOC_SYS_CFG bit, cleared by reading CTRL_WR
#define PIOC_REPLY_PENDING 6   // PIOC_SYS_CFG bit, cleared by reading CTRL_RD

#define PIOC_CAN_ACKED   0
#define PIOC_CAN_NO_ACK  1
#define PIOC_CAN_BUSY    2
#define PIOC_CAN_ERROR   3
#define PIOC_CAN_READY   0x80

#endif
