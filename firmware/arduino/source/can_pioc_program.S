#include "can_pioc_regs.h"

// RISC8B instructions encoded with GNU as so the existing toolchain can build
// both the host and PIOC code. Branch targets are PIOC word addresses.
.macro P_JMP target
    .short 0x6000 | ((\target - colorcan_pioc_program) / 2)
.endm
.macro P_JNZ target
    .short 0x3000 | ((\target - colorcan_pioc_program) / 2)
.endm
.macro P_JNC target
    .short 0x3800 | ((\target - colorcan_pioc_program) / 2)
.endm
.macro P_MOV reg
    .short 0x0200 | \reg
.endm
.macro P_MOVA reg
    .short 0x1000 | \reg
.endm
.macro P_MOVL value
    .short 0x2800 | \value
.endm
.macro P_CLR reg
    .short 0x0100 | \reg
.endm
.macro P_INC reg
    .short 0x1400 | \reg
.endm
.macro P_DEC reg
    .short 0x1500 | \reg
.endm
.macro P_XOR reg
    .short 0x0b00 | \reg
.endm
.macro P_RCL reg
    .short 0x1e00 | \reg
.endm
.macro P_BS reg, bit
    .short 0x4800 | (\bit << 8) | \reg
.endm
.macro P_BTSS reg, bit
    .short 0x5800 | (\bit << 8) | \reg
.endm
.macro P_DELAY cycles
    .zero 2 * \cycles   // RISC8B NOP is 0x0000
.endm

.equ frame_bits_left, 0x33
.equ byte_bits_left,  0x34
.equ shift_byte,      0x35
.equ result,          0x36
.equ idle_samples,    0x37

.section .rodata.colorcan_pioc_program,"a",@progbits
.balign 4
.global colorcan_pioc_program
.global colorcan_pioc_program_end
colorcan_pioc_program:
    .zero 8                       // WCH reset vector is at word 4
    P_JMP start

start:
    P_BS PIOC_PORT_IO, PIOC_TX_BIT // Set recessive before enabling the output.
    P_BS PIOC_PORT_DIR, PIOC_TX_BIT
    P_MOVL PIOC_CAN_READY
    P_MOVA PIOC_CTRL_RD

wait_for_frame:
    P_BTSS PIOC_SYS_CFG, PIOC_CMD_PENDING
    P_JMP wait_for_frame
    P_MOV PIOC_CTRL_WR             // Command byte is the bit count.
    P_MOVA frame_bits_left
    P_CLR PIOC_BIT_INDEX
    P_MOVL PIOC_CAN_NO_ACK
    P_MOVA result
    P_MOVL PIOC_FRAME_DATA
    P_MOVA PIOC_INDIR_ADDR
    P_MOV PIOC_INDIR_PORT
    P_MOVA shift_byte
    P_MOVL 8
    P_MOVA byte_bits_left

    // Sample the bus every five clocks over at least 11 recessive bit times.
    P_MOVL 107
    P_MOVA idle_samples
check_idle:
    P_BTSS PIOC_PORT_IO, PIOC_RX_INPUT
    P_JMP bus_busy
    P_DEC idle_samples
    P_JNZ check_idle

send_bit:
    P_RCL shift_byte               // Next MSB into carry.
    .short 0x00e0                  // BG2F 0,0: carry to DATA_EXCH bit 0.
    .short 0x00b8                  // BP2F 3,0: DATA_EXCH bit 0 to IO1 at cycle 0.

    // Refill on byte boundaries. Both paths reach sample_delay at cycle 10.
    P_DEC byte_bits_left
    P_JNZ no_refill
    P_INC PIOC_INDIR_ADDR
    P_MOV PIOC_INDIR_PORT
    P_MOVA shift_byte
    P_MOVL 8
    P_MOVA byte_bits_left
    P_JMP sample_delay
no_refill:
    P_DELAY 6
sample_delay:
    P_MOV PIOC_BIT_INDEX
    P_XOR PIOC_ACK_INDEX           // Z identifies the ACK slot. Carry still holds TX.
    P_DELAY 24
    .short 0x001c                  // BCTC 0: carry ^= IO0 at cycle 36 (75%).
    P_JNC matched
    P_JNZ mismatch                 // RX differs from TX outside the ACK slot.
    P_CLR result                   // Dominant RX in the ACK slot is a valid ACK.
    P_JMP advance
matched:
    P_DELAY 3
advance:                           // Both continuing paths reach this at cycle 42.
    P_INC PIOC_BIT_INDEX
    P_DEC frame_bits_left
    P_JNZ send_bit                 // Next IO1 update is at cycle 48.
    P_MOV result
    P_MOVA PIOC_CTRL_RD
    P_JMP wait_for_frame

mismatch:
    P_BS PIOC_PORT_IO, PIOC_TX_BIT // Release the bus and preserve the failing bit index.
    P_MOVL PIOC_CAN_ERROR
    P_MOVA PIOC_CTRL_RD
    P_JMP wait_for_frame

bus_busy:
    P_MOVL PIOC_CAN_BUSY
    P_MOVA PIOC_CTRL_RD
    P_JMP wait_for_frame

.if (. - colorcan_pioc_program) > 2048
    .error "PIOC code exceeds the conditional-branch address range"
.endif
.balign 4
colorcan_pioc_program_end:
