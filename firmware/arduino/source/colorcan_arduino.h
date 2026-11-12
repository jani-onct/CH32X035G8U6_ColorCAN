#ifndef COLORCAN_ARDUINO_H
#define COLORCAN_ARDUINO_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <Arduino.h>

void colorcanArduinoRun(void);
void colorcanArduinoPinMode(uint8_t pin, bool released);
void colorcanArduinoWrite(uint8_t pin, uint8_t level);
uint8_t colorcanArduinoRead(uint8_t pin);
void colorcanArduinoDelayUs(uint32_t us);
void colorcanArduinoDelayMs(uint32_t ms);
uint32_t colorcanArduinoTicks(void);
int colorcanArduinoPrintf(const char *format, ...);

// GPIO CFGXR field encodings for the shared PIOC driver.
#define GPIO_CFGLR_IN_FLOAT        4u
#define GPIO_CFGLR_OUT_2Mhz_PP     2u
#define GPIO_CFGLR_OUT_10Mhz_PP    1u
#define GPIO_CFGLR_OUT_10Mhz_AF_PP 9u

// PIOC_RM is bit 23 in AFIO_PCFR1 (CH32X035 reference manual, section 8.3.2.1).
#undef AFIO_PCFR1_PIOC_REMAP
#define AFIO_PCFR1_PIOC_REMAP (1u << 23)

#define FUNCONF_SYSTEM_CORE_CLOCK F_CPU
#define funPinMode(pin, mode) colorcanArduinoPinMode((pin), (mode) == GPIO_CFGLR_IN_FLOAT)
#define funDigitalWrite colorcanArduinoWrite
#define funDigitalRead colorcanArduinoRead
#define Delay_Us colorcanArduinoDelayUs
#define Delay_Ms colorcanArduinoDelayMs
#define funSysTick32 colorcanArduinoTicks
#define printf colorcanArduinoPrintf
#define poll_input() ((void)0)

#endif
