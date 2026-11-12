#ifdef ARDUINO_ARCH_CH32
#include "colorcan_arduino.h"
#else
#include "ch32fun.h"
#endif
#include "can_bench.h"
#include "can_pioc_regs.h"
#include "can_wire.h"
#include <stdio.h>

#define PIOC_SRAM_BASE 0x20004000u
#define PIOC_REG(offset) (*(volatile uint8_t *)(PIOC_BASE + (offset)))
#define PIOC_CLOCK_GATE (1u << 0)
#define PIOC_RESET (1u << 1)
#define PIOC_ENABLE_IO0 (1u << 2)
#define PIOC_ENABLE_IO1 (1u << 3)
#define PIOC_REPLY_TIMEOUT (300u * 48u)
#define TX_GPIO_MODE_SHIFT 12

_Static_assert(FUNCONF_SYSTEM_CORE_CLOCK == 48000000u, "PIOC timing expects 48 MHz HCLK");
_Static_assert(PIOC_FRAME_DATA + (CAN_WIRE_MAX_BITS + 7) / 8 <= PIOC_ACK_INDEX,
               "CAN frame must fit in the PIOC data registers");
_Static_assert(PIOC_CAN_ACKED == CAN_TX_ACKED && PIOC_CAN_NO_ACK == CAN_TX_NO_ACK &&
               PIOC_CAN_BUSY == CAN_TX_BUSY && PIOC_CAN_ERROR == CAN_TX_ERROR,
               "PIOC replies must match can_tx_result");

extern const uint32_t colorcan_pioc_program[], colorcan_pioc_program_end[];

static bool pioc_ready;
static const char *fault;
static int fault_bit = -1;

static void set_tx_mode(uint32_t mode)
{
    GPIOC->CFGXR = (GPIOC->CFGXR & ~(0xfu << TX_GPIO_MODE_SHIFT))
                 | (mode << TX_GPIO_MODE_SHIFT);
}

static void stop_pioc(void)
{
    // Hand TX back to the host's recessive output even if the PIOC timed out mid-bit.
    GPIOC->BSHR = 1u << 19;
    set_tx_mode(GPIO_CFGLR_OUT_10Mhz_PP);
    PIOC_REG(PIOC_SYS_CFG) = PIOC_RESET;
    pioc_ready = false;
}

static bool read_reply(uint8_t *reply)
{
    uint32_t start = funSysTick32();
    while (!(PIOC_REG(PIOC_SYS_CFG) & (1u << PIOC_REPLY_PENDING))) {
        if ((uint32_t)(funSysTick32() - start) >= PIOC_REPLY_TIMEOUT) return false;
    }
    *reply = PIOC_REG(PIOC_CTRL_RD);
    return true;
}

void can_bench_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
    RCC->AHBPCENR |= RCC_AHBPeriph_IO2W;
    stop_pioc();
    GPIOC->CFGXR = (GPIOC->CFGXR & ~(0xfu << 8)) | ((uint32_t)GPIO_CFGLR_IN_FLOAT << 8);
    AFIO->PCFR1 = (AFIO->PCFR1 & ~(AFIO_PCFR1_SWJ_CFG | AFIO_PCFR1_PIOC_REMAP))
                | AFIO_PCFR1_SWJ_CFG_DISABLE;

    volatile uint32_t *code_ram = (volatile uint32_t *)PIOC_SRAM_BASE;
    unsigned code_size = (uintptr_t)colorcan_pioc_program_end - (uintptr_t)colorcan_pioc_program;
    for (unsigned i = 0; i < code_size / sizeof(*code_ram); ++i) {
        code_ram[i] = colorcan_pioc_program[i];
    }
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static bool start_pioc(void)
{
    // Keep PC19 driven high by GPIO until the PIOC has set up its output latch.
    (void)PIOC_REG(PIOC_CTRL_RD);
    PIOC_REG(PIOC_SYS_CFG) = PIOC_CLOCK_GATE | PIOC_ENABLE_IO0;
    uint8_t reply;
    if (!read_reply(&reply) || reply != PIOC_CAN_READY) {
        stop_pioc();
        return false;
    }
    PIOC_REG(PIOC_SYS_CFG) = PIOC_CLOCK_GATE | PIOC_ENABLE_IO0 | PIOC_ENABLE_IO1;
    set_tx_mode(GPIO_CFGLR_OUT_10Mhz_AF_PP);
    pioc_ready = true;
    return true;
}

enum can_tx_result can_bench_send(uint16_t id, const uint8_t data[8])
{
    struct can_wire_frame frame;
    fault = NULL;
    fault_bit = -1;
    if (!can_encode_base(&frame, id, data, 8)) {
        fault = "invalid frame";
        return CAN_TX_ERROR;
    }
    if (!pioc_ready && !start_pioc()) {
        fault = "startup handshake";
        return CAN_TX_ERROR;
    }

    uint8_t packed[(CAN_WIRE_MAX_BITS + 7) / 8] = {0};
    for (unsigned i = 0; i < frame.length; ++i) {
        packed[i / 8] |= frame.bits[i] << (7 - (i % 8));
    }
    for (unsigned i = 0; i < sizeof(packed); ++i) {
        PIOC_REG(PIOC_FRAME_DATA + i) = packed[i];
    }
    PIOC_REG(PIOC_ACK_INDEX) = frame.ack_index;
    __asm__ volatile("fence iorw, iorw" ::: "memory");
    PIOC_REG(PIOC_CTRL_WR) = frame.length;

    uint8_t reply;
    if (!read_reply(&reply) || reply > PIOC_CAN_ERROR) {
        stop_pioc();
        fault = "transmit response";
        return CAN_TX_ERROR;
    }
    if (reply == PIOC_CAN_ERROR) {
        fault_bit = PIOC_REG(PIOC_BIT_INDEX);
    }
    return (enum can_tx_result)reply;
}

void can_bench_print_error(void)
{
    if (fault) printf("DIAG CAN PIOC %s\r\n", fault);
    else if (fault_bit >= 0) printf("DIAG CAN PIOC RX mismatch at bit %u\r\n", (unsigned)fault_bit);
}
