#ifdef ARDUINO_ARCH_CH32
#include "colorcan_arduino.h"
#define COLORCAN_CAN_BENCH
#else
#include "ch32fun.h"
#include "fsusb.h"
#endif
#include <stdbool.h>
#include <stdio.h>
#ifdef COLORCAN_CAN_BENCH
#include "can_bench.h"
#include <string.h>
#endif

// ColorCAN v1.0.0: U2 PA4=SCL, PA5=SDA. External pull-ups are fitted.
#define TCS_ADDR 0x29u
#define TCS_ID 0x44u
#define CMD 0x80u
#define AUTO_INCREMENT 0x20u

static uint8_t scl = PA4;
static uint8_t sda = PA5;

// Keep the output latch low. A released line is an input and rises through
// the board's 4.7 kOhm resistor. The MCU never actively drives a high level.
static void i2c_line(uint8_t pin, bool released)
{
    funPinMode(pin, released ? GPIO_CFGLR_IN_FLOAT : GPIO_CFGLR_OUT_2Mhz_PP);
}

static void half_period(void) { Delay_Us(40); }

static void clock_low(void)
{
    i2c_line(scl, false);
    Delay_Us(4);
}

static bool release_clock(void)
{
    i2c_line(scl, true);
    for (unsigned i = 0; i < 200; ++i) {
        if (funDigitalRead(scl)) {
            half_period();
            return true;
        }
        Delay_Us(5);
    }
    return false;
}

static bool i2c_start(void)
{
    i2c_line(sda, true);
    half_period();
    if (!release_clock() || !funDigitalRead(sda)) return false;
    i2c_line(sda, false);
    half_period();
    clock_low();
    return true;
}

static bool i2c_stop(void)
{
    clock_low();
    i2c_line(sda, false);
    half_period();
    bool clock_ok = release_clock();
    i2c_line(sda, true);
    half_period();
    return clock_ok && funDigitalRead(sda);
}

static bool i2c_write(uint8_t value)
{
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        i2c_line(sda, (value & mask) != 0);
        half_period();
        if (!release_clock()) return false;
        clock_low();
    }
    i2c_line(sda, true);
    half_period();
    if (!release_clock()) return false;
    bool acknowledged = !funDigitalRead(sda);
    clock_low();
    return acknowledged;
}

static bool i2c_read(uint8_t *value, bool acknowledge)
{
    uint8_t data = 0;
    i2c_line(sda, true);
    for (unsigned i = 0; i < 8; ++i) {
        half_period();
        if (!release_clock()) return false;
        data = (data << 1) | funDigitalRead(sda);
        clock_low();
    }
    i2c_line(sda, !acknowledge);
    half_period();
    if (!release_clock()) return false;
    clock_low();
    i2c_line(sda, true);
    *value = data;
    return true;
}

static bool sensor_read(uint8_t reg, uint8_t *data, unsigned length)
{
    bool ok = i2c_start() && i2c_write(TCS_ADDR << 1)
           && i2c_write(CMD | (length > 1 ? AUTO_INCREMENT : 0) | reg)
           && i2c_start() && i2c_write((TCS_ADDR << 1) | 1);
    for (unsigned i = 0; ok && i < length; ++i) {
        ok = i2c_read(&data[i], i + 1 < length);
    }
    return i2c_stop() && ok;
}

static bool sensor_write(uint8_t reg, uint8_t value)
{
    bool ok = i2c_start() && i2c_write(TCS_ADDR << 1)
           && i2c_write(CMD | reg) && i2c_write(value);
    return i2c_stop() && ok;
}

#ifndef COLORCAN_CAN_BENCH
static bool i2c_probe(uint8_t address)
{
    bool acknowledged = i2c_start() && i2c_write(address << 1);
    return i2c_stop() && acknowledged;
}

static bool check_line(uint8_t pin, uint8_t other)
{
    i2c_line(pin, false);
    half_period();
    bool ok = !funDigitalRead(pin) && funDigitalRead(other);
    i2c_line(pin, true);
    half_period();
    return ok && funDigitalRead(pin);
}

static void diagnose_bus(void)
{
    bool pa4 = check_line(PA4, PA5);
    bool pa5 = check_line(PA5, PA4);
    printf("DIAG GPIO PA4=%s PA5=%s\r\n", pa4 ? "OK" : "FAIL", pa5 ? "OK" : "FAIL");
    if (!pa4 || !pa5) return;

    scl = PA5;
    sda = PA4;
    uint8_t id;
    if (sensor_read(0x12, &id, 1) && id == TCS_ID) {
        printf("DIAG sensor detected with SCL=PA5 SDA=PA4\r\n");
        return;
    }
    scl = PA4;
    sda = PA5;

    unsigned count = 0;
    for (uint8_t address = 0x08; address <= 0x77; ++address) {
        if (i2c_probe(address)) {
            printf("DIAG I2C ACK at 0x%02X\r\n", (unsigned)address);
            ++count;
            Delay_Ms(10);
        }
    }
    if (count == 0) printf("DIAG GPIO OK; slow input-release scan: no devices\r\n");
}
#endif

static bool sensor_setup(void)
{
    // ATIME=0xEB: 50.4 ms. CONTROL=0: 1x gain. Start oscillator before ADC.
    if (!sensor_write(0x00, 0x01) || !sensor_write(0x01, 0xEB)
        || !sensor_write(0x0F, 0x00)) return false;
    Delay_Ms(4);
    return sensor_write(0x00, 0x03);
}

static unsigned le16(const uint8_t *data)
{
    return data[0] | ((unsigned)data[1] << 8);
}

#ifndef ARDUINO_ARCH_CH32
void HandleUSBInput(int count, uint8_t *data)
{
#ifdef COLORCAN_CAN_BENCH
    static const char command[] = "BOOTISP\n";
    if (count == (int)sizeof(command) - 1 && memcmp(data, command, sizeof(command) - 1) == 0) {
        USBFSReset();
        funRebootToBootloader();
    }
#else
    (void)count;
    (void)data;
#endif
}
#endif

static void hardware_init(void)
{
#ifndef ARDUINO_ARCH_CH32
    SystemInit();
    funGpioInitAll();
#endif
    i2c_line(scl, true);
    i2c_line(sda, true);
    funDigitalWrite(scl, 0);
    funDigitalWrite(sda, 0);
#ifndef ARDUINO_ARCH_CH32
    // Enable the supply-voltage detector used by ch32fun's X035 USB PHY setup.
    RCC->APB1PCENR |= RCC_APB1Periph_PWR;
    PWR->CTLR |= PWR_CTLR_PVDE;
#endif
#ifdef COLORCAN_CAN_BENCH
    can_bench_init();
#endif
#ifndef ARDUINO_ARCH_CH32
    USBFSSetup();
#endif
}

#ifndef COLORCAN_CAN_BENCH
int main(void)
{
    hardware_init();
    bool initialized = false;
    unsigned sample = 0;
    unsigned cycles = 0;
    while (1) {
        uint8_t id = 0;
        uint8_t status = 0;
        uint8_t rgbc[8];
        poll_input();
        if (!funDigitalRead(scl) || !funDigitalRead(sda)) {
            printf("ERR I2C bus held low SCL=%u SDA=%u\r\n",
                   (unsigned)funDigitalRead(scl), (unsigned)funDigitalRead(sda));
            initialized = false;
        } else if (!sensor_read(0x12, &id, 1)) {
            if (i2c_probe(TCS_ADDR)) {
                printf("ERR I2C 0x29 ACK, but ID read failed\r\n");
            } else {
                printf("ERR no ACK at I2C address 0x29\r\n");
                if (cycles % 8 == 0) {
                    Delay_Ms(10);
                    diagnose_bus();
                }
            }
            initialized = false;
        } else if (id != TCS_ID) {
            printf("ERR unexpected TCS34725 ID=0x%02X\r\n", (unsigned)id);
            initialized = false;
        } else if (!initialized) {
            initialized = sensor_setup();
            if (!initialized) printf("ERR TCS34725 initialization failed\r\n");
        } else if (!sensor_read(0x13, &status, 1)) {
            printf("ERR reading TCS34725 status\r\n");
            initialized = false;
        } else if (!(status & 1)) {
            printf("WAIT TCS34725 ID=0x44 AVALID=0\r\n");
        } else if (!sensor_read(0x14, rgbc, sizeof rgbc)) {
            printf("ERR reading RGBC registers\r\n");
            initialized = false;
        } else {
            printf("OK ID=0x44 #%u R=%u G=%u B=%u C=%u\r\n",
                   ++sample, le16(rgbc+2), le16(rgbc+4), le16(rgbc+6), le16(rgbc));
        }
        ++cycles;
        Delay_Ms(500);
    }
}
#else

enum sensor_flag {
    SENSOR_VALID = 1u << 0,
    SENSOR_I2C_ERROR = 1u << 1,
    SENSOR_BAD_ID = 1u << 2,
    SENSOR_SATURATED = 1u << 3,
};

static uint8_t read_sample(bool *initialized, uint8_t rgbc[8])
{
    uint8_t id, status;
    if (!sensor_read(0x12, &id, 1)) goto i2c_error;
    if (id != TCS_ID) {
        *initialized = false;
        return SENSOR_BAD_ID;
    }
    if (!*initialized) {
        if (!sensor_setup()) goto i2c_error;
        *initialized = true;
        return 0;
    }
    if (!sensor_read(0x13, &status, 1)) goto i2c_error;
    if (!(status & 1u)) return 0;
    if (!sensor_read(0x14, rgbc, 8)) goto i2c_error;
    for (unsigned i = 0; i < 8; i += 2) {
        if (le16(rgbc + i) >= 21504u) return SENSOR_VALID | SENSOR_SATURATED;
    }
    return SENSOR_VALID;

i2c_error:
    *initialized = false;
    return SENSOR_I2C_ERROR;
}

static uint8_t read_node(void)
{
    funPinMode(PA6, GPIO_CFGLR_IN_FLOAT);
    funPinMode(PA7, GPIO_CFGLR_IN_FLOAT);
    funPinMode(PB0, GPIO_CFGLR_IN_FLOAT);
    Delay_Ms(20);
    return (uint8_t)((!funDigitalRead(PA6)) | ((!funDigitalRead(PA7)) << 1)
                   | ((!funDigitalRead(PB0)) << 2));
}

static uint32_t uptime_ms(void)
{
    static uint32_t previous_tick, milliseconds, fractional_ticks;
    uint32_t tick = funSysTick32();
    fractional_ticks += tick - previous_tick;
    previous_tick = tick;
    milliseconds += fractional_ticks / 48000u;
    fractional_ticks %= 48000u;
    return milliseconds;
}

#ifdef ARDUINO_ARCH_CH32
void colorcanArduinoRun(void)
#else
int main(void)
#endif
{
    hardware_init();
    uint8_t node = read_node();
    bool initialized = false;
    uint8_t flags = 0;
    uint16_t samples = 0;
    uint16_t tx_counts[CAN_TX_RESULT_COUNT] = {0};
    uint32_t next_sample = 0, next_status = 0, next_usb = 0, next_usb_stats = 0;

    while (1) {
        uint32_t now = uptime_ms();
        poll_input();
        bool status_changed = false;
        if ((int32_t)(now - next_sample) >= 0) {
            next_sample = now + 100;
            uint8_t rgbc[8];
            uint8_t previous_flags = flags;
            flags = read_sample(&initialized, rgbc);
            status_changed = previous_flags != flags;
            if (flags & SENSOR_VALID) {
                ++samples;
                // TCS34725 register order is C, R, G, B; CAN order is R, G, B, C.
                uint8_t data[8] = {rgbc[2], rgbc[3], rgbc[4], rgbc[5],
                                   rgbc[6], rgbc[7], rgbc[0], rgbc[1]};
                ++tx_counts[can_bench_send(0x240u + node, data)];
                if ((int32_t)(now - next_usb) >= 0) {
                    printf("OK ID=0x44 #%u R=%u G=%u B=%u C=%u\r\n", (unsigned)samples,
                           le16(rgbc+2), le16(rgbc+4), le16(rgbc+6), le16(rgbc));
                    next_usb = now + 500;
                }
            } else if ((int32_t)(now - next_usb) >= 0) {
                printf("ERR sensor flags=0x%02X\r\n", (unsigned)flags);
                next_usb = now + 500;
            }
        }
        if (status_changed || (int32_t)(now - next_status) >= 0) {
            uint8_t data[8] = {1, flags, (uint8_t)samples, (uint8_t)(samples >> 8),
                               (uint8_t)now, (uint8_t)(now >> 8),
                               (uint8_t)(now >> 16), (uint8_t)(now >> 24)};
            ++tx_counts[can_bench_send(0x248u + node, data)];
            next_status = now + 1000;
        }
        if ((int32_t)(now - next_usb_stats) >= 0) {
            printf("CAN N=%u ACK=%u NACK=%u BUSY=%u ERR=%u\r\n", (unsigned)node,
                   (unsigned)tx_counts[CAN_TX_ACKED], (unsigned)tx_counts[CAN_TX_NO_ACK],
                   (unsigned)tx_counts[CAN_TX_BUSY], (unsigned)tx_counts[CAN_TX_ERROR]);
            Delay_Ms(2);
            can_bench_print_error();
            next_usb_stats = now + 1000;
        }
        Delay_Ms(1);
    }
}
#endif
